#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <cJSON.h>
#include <string.h>
#include "diagnostics.h"
#include "mqtt_client.h"
#include "robot_controller.h"
#include "stepper_manager.h"
#include "stepper_motor.h"
#include "stepper_config.h"
#include "servo_manager.h"
#include "servo_motor.h"

LOG_MODULE_REGISTER(diagnostics, LOG_LEVEL_INF);

/* ============================================================================
 * Helper functions
 * ============================================================================ */

static const char *stepper_id_to_name(stepper_id_t id)
{
    switch (id) {
    case STEPPER_ID_X_AXIS:  return "x";
    case STEPPER_ID_Y1_AXIS: return "y1";
    case STEPPER_ID_Y2_AXIS: return "y2";
    case STEPPER_ID_Z_AXIS:  return "z";
    default:                 return "unknown";
    }
}

static stepper_id_t stepper_name_to_id(const char *name)
{
    if (strcmp(name, "x") == 0 || strcmp(name, "X") == 0) return STEPPER_ID_X_AXIS;
    if (strcmp(name, "y") == 0 || strcmp(name, "Y") == 0 || strcmp(name, "y1") == 0 || strcmp(name, "Y1") == 0) return STEPPER_ID_Y1_AXIS;
    if (strcmp(name, "y2") == 0 || strcmp(name, "Y2") == 0) return STEPPER_ID_Y2_AXIS;
    if (strcmp(name, "z") == 0 || strcmp(name, "Z") == 0) return STEPPER_ID_Z_AXIS;
    return STEPPER_ID_MAX; /* invalid */
}

static void publish_diag_response(const char *topic, const char *status, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    
    cJSON_AddStringToObject(root, "status", status);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddNumberToObject(root, "timestamp", k_uptime_get_32());
    
    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        app_mqtt_publish(topic, payload, strlen(payload));
        cJSON_free(payload);
    }
    cJSON_Delete(root);
}

/* ============================================================================
 * Stepper diagnostics handlers
 * ============================================================================ */

static void on_diag_stepper_move(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"motor": "x", "steps": 200, "speed": 1000} */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        LOG_ERR("DIAG: Failed to parse stepper move JSON");
        publish_diag_response("chess/diag/stepper/response", "error", "Invalid JSON");
        return;
    }

    cJSON *motor_name = cJSON_GetObjectItem(root, "motor");
    cJSON *steps = cJSON_GetObjectItem(root, "steps");
    cJSON *speed = cJSON_GetObjectItem(root, "speed");

    if (!motor_name || !cJSON_IsString(motor_name)) {
        LOG_ERR("DIAG: Missing 'motor' field");
        publish_diag_response("chess/diag/stepper/response", "error", "Missing 'motor' field");
        cJSON_Delete(root);
        return;
    }

    if (!steps || !cJSON_IsNumber(steps)) {
        LOG_ERR("DIAG: Missing 'steps' field");
        publish_diag_response("chess/diag/stepper/response", "error", "Missing 'steps' field");
        cJSON_Delete(root);
        return;
    }

    uint32_t speed_us = (speed && cJSON_IsNumber(speed)) ? (uint32_t)speed->valueint : STEPPER_DEFAULT_SPEED_US;
    int32_t step_count = (int32_t)steps->valueint;

    /* Special handling: "y" targets the dual-drive pair (y1 + y2) */
    if (strcmp(motor_name->valuestring, "y") == 0 || strcmp(motor_name->valuestring, "Y") == 0) {
        stepper_motor_t *y1 = stepper_manager_get_motor(STEPPER_ID_Y1_AXIS);
        stepper_motor_t *y2 = stepper_manager_get_motor(STEPPER_ID_Y2_AXIS);
        if (!y1 || !y2) {
            publish_diag_response("chess/diag/stepper/response", "error", "Y pair not registered");
            cJSON_Delete(root);
            return;
        }

        int ret = stepper_motor_move_steps_sync(y1, y2, step_count, speed_us);
        if (ret < 0) {
            LOG_ERR("DIAG: Failed to move Y pair: %d", ret);
            publish_diag_response("chess/diag/stepper/response", "error", "Move failed (check enable)");
        } else {
            LOG_INF("DIAG: Moving Y pair by %d steps at %u us/step", step_count, speed_us);
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "status", "ok");
            cJSON_AddStringToObject(resp, "motor", "y");
            cJSON_AddNumberToObject(resp, "steps", step_count);
            cJSON_AddNumberToObject(resp, "speed_us", speed_us);
            cJSON_AddNumberToObject(resp, "timestamp", k_uptime_get_32());
            char *resp_payload = cJSON_PrintUnformatted(resp);
            if (resp_payload) {
                app_mqtt_publish("chess/diag/stepper/response", resp_payload, strlen(resp_payload));
                cJSON_free(resp_payload);
            }
            cJSON_Delete(resp);
        }
        cJSON_Delete(root);
        return;
    }

    stepper_id_t id = stepper_name_to_id(motor_name->valuestring);
    if (id >= STEPPER_ID_MAX) {
        LOG_ERR("DIAG: Unknown motor '%s'", motor_name->valuestring);
        publish_diag_response("chess/diag/stepper/response", "error", "Unknown motor");
        cJSON_Delete(root);
        return;
    }

    stepper_motor_t *motor = stepper_manager_get_motor(id);
    if (!motor) {
        LOG_ERR("DIAG: Motor %s not registered", motor_name->valuestring);
        publish_diag_response("chess/diag/stepper/response", "error", "Motor not registered");
        cJSON_Delete(root);
        return;
    }

    int ret = stepper_motor_move_steps(motor, step_count, speed_us);
    if (ret < 0) {
        LOG_ERR("DIAG: Failed to move motor %s: %d", motor_name->valuestring, ret);
        publish_diag_response("chess/diag/stepper/response", "error", "Move failed (check enable)");
    } else {
        LOG_INF("DIAG: Moving motor %s by %d steps at %u us/step", 
                motor_name->valuestring, step_count, speed_us);
        
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "status", "ok");
        cJSON_AddStringToObject(resp, "motor", motor_name->valuestring);
        cJSON_AddNumberToObject(resp, "steps", step_count);
        cJSON_AddNumberToObject(resp, "speed_us", speed_us);
        cJSON_AddNumberToObject(resp, "timestamp", k_uptime_get_32());
        char *resp_payload = cJSON_PrintUnformatted(resp);
        if (resp_payload) {
            app_mqtt_publish("chess/diag/stepper/response", resp_payload, strlen(resp_payload));
            cJSON_free(resp_payload);
        }
        cJSON_Delete(resp);
    }

    cJSON_Delete(root);
}

static void on_diag_stepper_stop(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"motor": "x"} or {"motor": "all"} */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        stepper_manager_stop_all();
        LOG_INF("DIAG: Emergency stop all motors (no JSON)");
        publish_diag_response("chess/diag/stepper/response", "ok", "All motors stopped");
        return;
    }

    cJSON *motor_name = cJSON_GetObjectItem(root, "motor");
    if (!motor_name || !cJSON_IsString(motor_name)) {
        stepper_manager_stop_all();
        LOG_INF("DIAG: Stopping all motors");
        publish_diag_response("chess/diag/stepper/response", "ok", "All motors stopped");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(motor_name->valuestring, "all") == 0) {
        stepper_manager_stop_all();
        LOG_INF("DIAG: Stopping all motors");
        publish_diag_response("chess/diag/stepper/response", "ok", "All motors stopped");
    } else if (strcmp(motor_name->valuestring, "y") == 0 || strcmp(motor_name->valuestring, "Y") == 0) {
        stepper_motor_t *y1 = stepper_manager_get_motor(STEPPER_ID_Y1_AXIS);
        stepper_motor_t *y2 = stepper_manager_get_motor(STEPPER_ID_Y2_AXIS);
        if (y1) stepper_motor_stop(y1);
        if (y2) stepper_motor_stop(y2);
        LOG_INF("DIAG: Stopped Y pair");
        publish_diag_response("chess/diag/stepper/response", "ok", "Y pair stopped");
    } else {
        stepper_id_t id = stepper_name_to_id(motor_name->valuestring);
        if (id < STEPPER_ID_MAX) {
            stepper_motor_t *motor = stepper_manager_get_motor(id);
            if (motor) {
                stepper_motor_stop(motor);
                LOG_INF("DIAG: Stopped motor %s", motor_name->valuestring);
                publish_diag_response("chess/diag/stepper/response", "ok", "Motor stopped");
            }
        }
    }

    cJSON_Delete(root);
}

static void on_diag_stepper_status(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"motor": "x"} or {} for all motors */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    cJSON *resp = cJSON_CreateObject();
    if (!resp) {
        if (root) cJSON_Delete(root);
        return;
    }

    cJSON_AddStringToObject(resp, "type", "stepper_status");
    cJSON_AddNumberToObject(resp, "timestamp", k_uptime_get_32());

    cJSON *motor_name = root ? cJSON_GetObjectItem(root, "motor") : NULL;

    if (motor_name && cJSON_IsString(motor_name) && strcmp(motor_name->valuestring, "all") != 0) {
        /* Single motor or Y pair status */
        if (strcmp(motor_name->valuestring, "y") == 0 || strcmp(motor_name->valuestring, "Y") == 0) {
            stepper_motor_t *y1 = stepper_manager_get_motor(STEPPER_ID_Y1_AXIS);
            stepper_motor_t *y2 = stepper_manager_get_motor(STEPPER_ID_Y2_AXIS);
            cJSON *m = cJSON_CreateObject();
            if (y1 && y2) {
                cJSON_AddNumberToObject(m, "position_y1", stepper_motor_get_position(y1));
                cJSON_AddNumberToObject(m, "position_y2", stepper_motor_get_position(y2));
                cJSON_AddBoolToObject(m, "moving_y1", stepper_motor_is_moving(y1));
                cJSON_AddBoolToObject(m, "moving_y2", stepper_motor_is_moving(y2));
                cJSON_AddBoolToObject(m, "aligned", stepper_motor_get_position(y1) == stepper_motor_get_position(y2));
            } else {
                cJSON_AddStringToObject(m, "status", "error");
                cJSON_AddStringToObject(m, "message", "Y pair not found");
            }
            cJSON_AddItemToObject(resp, "motor", cJSON_CreateString("y"));
            cJSON_AddItemToObject(resp, "y_pair", m);
        } else {
            stepper_id_t id = stepper_name_to_id(motor_name->valuestring);
            stepper_motor_t *motor = (id < STEPPER_ID_MAX) ? stepper_manager_get_motor(id) : NULL;
            
            if (motor) {
                cJSON_AddStringToObject(resp, "motor", motor_name->valuestring);
                cJSON_AddNumberToObject(resp, "position", stepper_motor_get_position(motor));
                cJSON_AddBoolToObject(resp, "moving", stepper_motor_is_moving(motor));
                cJSON_AddNumberToObject(resp, "state", stepper_motor_get_state(motor));
            } else {
                cJSON_AddStringToObject(resp, "status", "error");
                cJSON_AddStringToObject(resp, "message", "Motor not found");
            }
        }
    } else {
        /* All motors status */
        cJSON *motors = cJSON_CreateObject();
        for (int i = 0; i < STEPPER_ID_MAX; i++) {
            stepper_motor_t *motor = stepper_manager_get_motor(i);
            if (motor) {
                cJSON *m = cJSON_CreateObject();
                cJSON_AddNumberToObject(m, "position", stepper_motor_get_position(motor));
                cJSON_AddBoolToObject(m, "moving", stepper_motor_is_moving(motor));
                cJSON_AddNumberToObject(m, "state", stepper_motor_get_state(motor));
                cJSON_AddItemToObject(motors, stepper_id_to_name(i), m);
            }
        }
        cJSON_AddItemToObject(resp, "motors", motors);
        cJSON_AddBoolToObject(resp, "all_idle", stepper_manager_all_idle());
    }

    char *resp_payload = cJSON_PrintUnformatted(resp);
    if (resp_payload) {
        app_mqtt_publish("chess/diag/stepper/response", resp_payload, strlen(resp_payload));
        cJSON_free(resp_payload);
    }
    cJSON_Delete(resp);
    if (root) cJSON_Delete(root);
}

static void on_diag_stepper_enable(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"motor": "x", "enable": true} or {"motor": "all", "enable": true} */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        LOG_ERR("DIAG: Failed to parse stepper enable JSON");
        publish_diag_response("chess/diag/stepper/response", "error", "Invalid JSON");
        return;
    }

    cJSON *motor_name = cJSON_GetObjectItem(root, "motor");
    cJSON *enable = cJSON_GetObjectItem(root, "enable");

    if (!enable || !cJSON_IsBool(enable)) {
        LOG_ERR("DIAG: Missing 'enable' field");
        publish_diag_response("chess/diag/stepper/response", "error", "Missing 'enable' field");
        cJSON_Delete(root);
        return;
    }

    bool en = cJSON_IsTrue(enable);

    if (!motor_name || !cJSON_IsString(motor_name) || strcmp(motor_name->valuestring, "all") == 0) {
        int ret = stepper_manager_enable_all(en);
        if (ret < 0) {
            publish_diag_response("chess/diag/stepper/response", "error", "Enable all failed");
        } else {
            LOG_INF("DIAG: %s all motors", en ? "Enabled" : "Disabled");
            publish_diag_response("chess/diag/stepper/response", "ok", 
                                  en ? "All motors enabled" : "All motors disabled");
        }
    } else if (strcmp(motor_name->valuestring, "y") == 0 || strcmp(motor_name->valuestring, "Y") == 0) {
        stepper_motor_t *y1 = stepper_manager_get_motor(STEPPER_ID_Y1_AXIS);
        stepper_motor_t *y2 = stepper_manager_get_motor(STEPPER_ID_Y2_AXIS);
        int ret1 = y1 ? stepper_motor_enable(y1, en) : -ENODEV;
        int ret2 = y2 ? stepper_motor_enable(y2, en) : -ENODEV;
        if (ret1 < 0 || ret2 < 0) {
            publish_diag_response("chess/diag/stepper/response", "error", "Enable Y pair failed");
        } else {
            LOG_INF("DIAG: %s Y pair", en ? "Enabled" : "Disabled");
            publish_diag_response("chess/diag/stepper/response", "ok", 
                                  en ? "Y pair enabled" : "Y pair disabled");
        }
    } else {
        stepper_id_t id = stepper_name_to_id(motor_name->valuestring);
        stepper_motor_t *motor = (id < STEPPER_ID_MAX) ? stepper_manager_get_motor(id) : NULL;
        
        if (motor) {
            int ret = stepper_motor_enable(motor, en);
            if (ret < 0) {
                publish_diag_response("chess/diag/stepper/response", "error", "Enable failed");
            } else {
                LOG_INF("DIAG: %s motor %s", en ? "Enabled" : "Disabled", motor_name->valuestring);
                publish_diag_response("chess/diag/stepper/response", "ok", 
                                      en ? "Motor enabled" : "Motor disabled");
            }
        } else {
            publish_diag_response("chess/diag/stepper/response", "error", "Motor not found");
        }
    }

    cJSON_Delete(root);
}

static void on_diag_stepper_home(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"motor": "x"} or {"motor": "all"} - sets current position as 0 (no physical movement) */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    cJSON *motor_name = root ? cJSON_GetObjectItem(root, "motor") : NULL;

    if (!motor_name || !cJSON_IsString(motor_name) || strcmp(motor_name->valuestring, "all") == 0) {
        for (int i = 0; i < STEPPER_ID_MAX; i++) {
            stepper_motor_t *motor = stepper_manager_get_motor(i);
            if (motor) {
                stepper_motor_set_position(motor, 0);
            }
        }
        LOG_INF("DIAG: Zeroed all motor positions");
        publish_diag_response("chess/diag/stepper/response", "ok", "All motor positions zeroed");
    } else if (strcmp(motor_name->valuestring, "y") == 0 || strcmp(motor_name->valuestring, "Y") == 0) {
        stepper_motor_t *y1 = stepper_manager_get_motor(STEPPER_ID_Y1_AXIS);
        stepper_motor_t *y2 = stepper_manager_get_motor(STEPPER_ID_Y2_AXIS);
        if (y1) stepper_motor_set_position(y1, 0);
        if (y2) stepper_motor_set_position(y2, 0);
        LOG_INF("DIAG: Zeroed Y pair positions");
        publish_diag_response("chess/diag/stepper/response", "ok", "Y pair positions zeroed");
    } else {
        stepper_id_t id = stepper_name_to_id(motor_name->valuestring);
        stepper_motor_t *motor = (id < STEPPER_ID_MAX) ? stepper_manager_get_motor(id) : NULL;
        
        if (motor) {
            stepper_motor_set_position(motor, 0);
            LOG_INF("DIAG: Zeroed motor %s position", motor_name->valuestring);
            publish_diag_response("chess/diag/stepper/response", "ok", "Motor position zeroed");
        } else {
            publish_diag_response("chess/diag/stepper/response", "error", "Motor not found");
        }
    }

    if (root) cJSON_Delete(root);
}

/* ============================================================================
 * Homing diagnostics handlers
 * ============================================================================ */

static void on_diag_homing_start(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"axis": "x"} or {"axis": "all"} */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    cJSON *axis_name = root ? cJSON_GetObjectItem(root, "axis") : NULL;
    int ret;

    if (!axis_name || !cJSON_IsString(axis_name) || strcmp(axis_name->valuestring, "all") == 0) {
        ret = robot_controller_home_all();
        if (ret < 0) {
            LOG_ERR("DIAG: Failed to start homing all axes: %d", ret);
            publish_diag_response("chess/diag/homing/response", "error", "Failed to start homing");
        } else {
            LOG_INF("DIAG: Started homing all axes");
            publish_diag_response("chess/diag/homing/response", "ok", "Homing started (Z -> Y -> X)");
        }
    } else {
        char axis = axis_name->valuestring[0];
        ret = robot_controller_home_axis(axis);
        if (ret < 0) {
            LOG_ERR("DIAG: Failed to start homing axis %c: %d", axis, ret);
            publish_diag_response("chess/diag/homing/response", "error", "Failed to start homing axis");
        } else {
            LOG_INF("DIAG: Started homing axis %c", axis);
            
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "status", "ok");
            cJSON_AddStringToObject(resp, "axis", axis_name->valuestring);
            cJSON_AddStringToObject(resp, "message", "Homing started");
            cJSON_AddNumberToObject(resp, "timestamp", k_uptime_get_32());
            char *resp_payload = cJSON_PrintUnformatted(resp);
            if (resp_payload) {
                app_mqtt_publish("chess/diag/homing/response", resp_payload, strlen(resp_payload));
                cJSON_free(resp_payload);
            }
            cJSON_Delete(resp);
        }
    }

    if (root) cJSON_Delete(root);
}

static void on_diag_homing_status(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Returns current homing state */
    cJSON *resp = cJSON_CreateObject();
    if (!resp) return;

    cJSON_AddStringToObject(resp, "type", "homing_status");
    cJSON_AddNumberToObject(resp, "timestamp", k_uptime_get_32());
    
    /* Homing state */
    homing_state_t state = robot_controller_get_homing_state();
    const char *state_str;
    switch (state) {
        case HOMING_STATE_IDLE: state_str = "idle"; break;
        case HOMING_STATE_X: state_str = "homing_x"; break;
        case HOMING_STATE_Y: state_str = "homing_y"; break;
        case HOMING_STATE_Z: state_str = "homing_z"; break;
        case HOMING_STATE_COMPLETE: state_str = "complete"; break;
        case HOMING_STATE_ERROR: state_str = "error"; break;
        default: state_str = "unknown"; break;
    }
    cJSON_AddStringToObject(resp, "homing_state", state_str);
    cJSON_AddBoolToObject(resp, "is_homing", robot_controller_is_homing());

    char *resp_payload = cJSON_PrintUnformatted(resp);
    if (resp_payload) {
        app_mqtt_publish("chess/diag/homing/response", resp_payload, strlen(resp_payload));
        cJSON_free(resp_payload);
    }
    cJSON_Delete(resp);
}

/* ============================================================================
 * Servo diagnostics handlers
 * ============================================================================ */

static void on_diag_servo_set(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"angle": 90} */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        LOG_ERR("DIAG: Failed to parse servo set JSON");
        publish_diag_response("chess/diag/servo/response", "error", "Invalid JSON");
        return;
    }

    cJSON *angle = cJSON_GetObjectItem(root, "angle");

    if (!angle || !cJSON_IsNumber(angle)) {
        publish_diag_response("chess/diag/servo/response", "error", "Missing 'angle' field");
        cJSON_Delete(root);
        return;
    }

    /* Auto-enable the single gripper servo before setting angle */
    int ret = robot_controller_servo_enable(SERVO_ID_1, true);
    if (ret < 0) {
        LOG_ERR("DIAG: Failed to enable gripper servo before set: %d", ret);
        publish_diag_response("chess/diag/servo/response", "error", "Failed to enable servo");
        cJSON_Delete(root);
        return;
    }

    ret = robot_controller_servo_set_angle(SERVO_ID_1, (uint16_t)angle->valueint);
    if (ret < 0) {
        LOG_ERR("DIAG: Failed to set gripper servo angle: %d", ret);
        publish_diag_response("chess/diag/servo/response", "error", "Failed to set angle");
    } else {
        LOG_INF("DIAG: Set gripper servo to %d degrees", angle->valueint);
        
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "status", "ok");
        cJSON_AddNumberToObject(resp, "angle", angle->valueint);
        cJSON_AddNumberToObject(resp, "timestamp", k_uptime_get_32());
        char *resp_payload = cJSON_PrintUnformatted(resp);
        if (resp_payload) {
            app_mqtt_publish("chess/diag/servo/response", resp_payload, strlen(resp_payload));
            cJSON_free(resp_payload);
        }
        cJSON_Delete(resp);
    }

    cJSON_Delete(root);
}

static void on_diag_servo_enable(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    /* Expected JSON: {"enable": true} */
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        LOG_ERR("DIAG: Failed to parse servo enable JSON");
        publish_diag_response("chess/diag/servo/response", "error", "Invalid JSON");
        return;
    }

    cJSON *enable = cJSON_GetObjectItem(root, "enable");

    if (!enable || !cJSON_IsBool(enable)) {
        publish_diag_response("chess/diag/servo/response", "error", "Missing 'enable' field");
        cJSON_Delete(root);
        return;
    }

    bool en = cJSON_IsTrue(enable);
    int ret = robot_controller_servo_enable(SERVO_ID_1, en);
    if (ret < 0) {
        LOG_ERR("DIAG: Failed to %s gripper servo: %d", en ? "enable" : "disable", ret);
        publish_diag_response("chess/diag/servo/response", "error", "Failed to set enable");
    } else {
        LOG_INF("DIAG: %s gripper servo", en ? "Enabled" : "Disabled");
        publish_diag_response("chess/diag/servo/response", "ok", en ? "Servo enabled" : "Servo disabled");
    }

    cJSON_Delete(root);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int diagnostics_init(void)
{
    LOG_INF("Initializing diagnostics module");

    /* Stepper diagnostics */
    app_mqtt_subscribe("chess/diag/stepper/move", on_diag_stepper_move);
    app_mqtt_subscribe("chess/diag/stepper/stop", on_diag_stepper_stop);
    app_mqtt_subscribe("chess/diag/stepper/status", on_diag_stepper_status);
    app_mqtt_subscribe("chess/diag/stepper/enable", on_diag_stepper_enable);
    app_mqtt_subscribe("chess/diag/stepper/home", on_diag_stepper_home);

    /* Homing diagnostics */
    app_mqtt_subscribe("chess/diag/homing/start", on_diag_homing_start);
    app_mqtt_subscribe("chess/diag/homing/status", on_diag_homing_status);

    /* Servo diagnostics */
    app_mqtt_subscribe("chess/diag/servo/set", on_diag_servo_set);
    app_mqtt_subscribe("chess/diag/servo/enable", on_diag_servo_enable);

    LOG_INF("Diagnostics module initialized");
    return 0;
}
