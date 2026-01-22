#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <cJSON.h>
#include <string.h>
#include "board_manager.h"
#include "mqtt_client.h"
#include "robot_controller.h"
#include "diagnostics.h"

LOG_MODULE_REGISTER(application, LOG_LEVEL_INF);

#define BOARD_SCAN_INTERVAL_MS 100

/*
 * Event Handlers
*/
static void on_move_detected(const board_move_t *move)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERR("Failed to create JSON object");
        return;
    }

    cJSON_AddStringToObject(root, "type", "move");

    cJSON *from = cJSON_CreateObject();
    cJSON_AddNumberToObject(from, "row", move->from.row);
    cJSON_AddNumberToObject(from, "col", move->from.col);
    cJSON_AddItemToObject(root, "from", from);

    cJSON *to = cJSON_CreateObject();
    cJSON_AddNumberToObject(to, "row", move->to.row);
    cJSON_AddNumberToObject(to, "col", move->to.col);
    cJSON_AddItemToObject(root, "to", to);

    cJSON_AddNumberToObject(root, "timestamp", move->timestamp);

    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        int rc = app_mqtt_publish("chess/board/move", payload, strlen(payload));
        if (rc < 0) {
            LOG_WRN("Failed to publish move (rc=%d) - MQTT connected: %s", 
                    rc, app_mqtt_is_connected() ? "yes" : "no");
        } else {
            LOG_INF("Published move to MQTT");
        }
        cJSON_free(payload);
    }

    cJSON_Delete(root);
}

static void on_state_changed(const chess_board_state_t *state)
{
    /* Publish compact state (hex mask, move count, timestamp) */
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERR("Failed to create JSON object");
        return;
    }

    cJSON_AddStringToObject(root, "type", "state");

    char mask_str[20];
    snprintf(mask_str, sizeof(mask_str), "0x%016llx", state->occupied_mask);
    cJSON_AddStringToObject(root, "occupied", mask_str);

    cJSON_AddNumberToObject(root, "moves", state->move_count);
    cJSON_AddNumberToObject(root, "timestamp", state->last_update_time);

    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        int rc = app_mqtt_publish("chess/board/state", payload, strlen(payload));
        if (rc < 0) {
            LOG_DBG("Failed to publish state (rc=%d) - MQTT connected: %s", 
                    rc, app_mqtt_is_connected() ? "yes" : "no");
        }
        cJSON_free(payload);
    }
    cJSON_Delete(root);

    /* Publish full board grid as array to chess/board/fullstate */
    cJSON *full = cJSON_CreateObject();
    if (!full) {
        LOG_ERR("Failed to create fullstate JSON object");
        return;
    }
    cJSON_AddStringToObject(full, "type", "fullstate");
    cJSON_AddNumberToObject(full, "timestamp", state->last_update_time);
    cJSON *rows = cJSON_CreateArray();
    for (int r = 0; r < CHESS_BOARD_SIZE; r++) {
        cJSON *row = cJSON_CreateArray();
        for (int c = 0; c < CHESS_BOARD_SIZE; c++) {
            int occ = (state->occupied_mask & (1ULL << (r * CHESS_BOARD_SIZE + c))) ? 1 : 0;
            cJSON_AddItemToArray(row, cJSON_CreateNumber(occ));
        }
        cJSON_AddItemToArray(rows, row);
    }
    cJSON_AddItemToObject(full, "board", rows);
    char *full_payload = cJSON_PrintUnformatted(full);
    if (full_payload) {
        int rc = app_mqtt_publish("chess/board/fullstate", full_payload, strlen(full_payload));
        if (rc < 0) {
            LOG_DBG("Failed to publish fullstate (rc=%d) - MQTT connected: %s", 
                    rc, app_mqtt_is_connected() ? "yes" : "no");
        }
        cJSON_free(full_payload);
    }
    cJSON_Delete(full);
}

/*
 * MQTT Message Handlers
*/
static void on_ping_received(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERR("Failed to create JSON object");
        return;
    }

    cJSON_AddStringToObject(root, "status", "pong");
    cJSON_AddNumberToObject(root, "timestamp", k_uptime_get_32());
    cJSON_AddBoolToObject(root, "robot_busy", robot_controller_is_busy());

    robot_position_t pos = robot_controller_get_position();
    cJSON *position = cJSON_CreateObject();
    cJSON_AddNumberToObject(position, "x", pos.x);
    cJSON_AddNumberToObject(position, "y", pos.y);
    cJSON_AddNumberToObject(position, "z", pos.z);
    cJSON_AddItemToObject(root, "position", position);

    char *response = cJSON_PrintUnformatted(root);
    if (response) {
        int rc = app_mqtt_publish("chess/system/pong", response, strlen(response));
        if (rc < 0) {
            LOG_WRN("Failed to publish pong (rc=%d) - MQTT connected: %s", 
                    rc, app_mqtt_is_connected() ? "yes" : "no");
        } else {
            LOG_INF("Responded to ping");
        }
        cJSON_free(response);
    }

    cJSON_Delete(root);
}

static void on_robot_command_received(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    cJSON *root = cJSON_ParseWithLength((const char *)payload, payload_len);
    if (!root) {
        LOG_ERR("Failed to parse robot command JSON");
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "command");
    if (!cmd || !cJSON_IsString(cmd)) {
        LOG_ERR("Invalid command format");
        cJSON_Delete(root);
        return;
    }

    const char *command = cmd->valuestring;
    LOG_INF("Robot command received: %s", command);

    if (strcmp(command, "move") == 0) {
        cJSON *x = cJSON_GetObjectItem(root, "x");
        cJSON *y = cJSON_GetObjectItem(root, "y");
        cJSON *z = cJSON_GetObjectItem(root, "z");
        cJSON *speed = cJSON_GetObjectItem(root, "speed");
        
        if (x && y && z) {
            uint32_t speed_us = speed && cJSON_IsNumber(speed) ? speed->valueint : 1000;
            robot_controller_move_to(x->valueint, y->valueint, z->valueint, speed_us);
            LOG_INF("Moving to X=%d Y=%d Z=%d", x->valueint, y->valueint, z->valueint);
        }
    } else if (strcmp(command, "home") == 0) {
        robot_controller_home();
        LOG_INF("Homing robot");
    } else if (strcmp(command, "gripper_open") == 0) {
        robot_controller_gripper_open();
        LOG_INF("Opening gripper");
    } else if (strcmp(command, "gripper_close") == 0) {
        robot_controller_gripper_close();
        LOG_INF("Closing gripper");
    }

    cJSON_Delete(root);
}

int application_init(void)
{
    int ret;

    LOG_INF("Initializing chess board application");

    ret = board_manager_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize board manager: %d", ret);
        return ret;
    }

    board_manager_register_move_callback(on_move_detected);
    board_manager_register_state_callback(on_state_changed);

    app_mqtt_subscribe("chess/system/ping", on_ping_received);
    app_mqtt_subscribe("chess/robot/command", on_robot_command_received);

    ret = diagnostics_init();
    if (ret < 0) {
        LOG_WRN("Failed to initialize diagnostics: %d", ret);
    }

    LOG_INF("Application initialized");
    return 0;
}

void application_task(void)
{
    while (1) {
        board_manager_update();
        k_sleep(K_MSEC(BOARD_SCAN_INTERVAL_MS));
    }
}
