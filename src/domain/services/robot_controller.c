#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "robot_controller.h"
#include "stepper_motor.h"
#include "stepper_manager.h"
#include "stepper_config.h"
#include "servo_motor.h"
#include "servo_manager.h"
#include "servo_config.h"
#include "limit_switch.h"
#include "movement_planner.h"
#include "robot_config.h"

LOG_MODULE_REGISTER(robot_controller, LOG_LEVEL_INF);

/* Homing configuration */
#define HOMING_SPEED_US      2000  /* Slower speed for homing (safety) */
#define HOMING_DIR_X         STEPPER_DIR_CCW  /* Direction towards X home switch */
#define HOMING_DIR_Y         STEPPER_DIR_CCW  /* Direction towards Y home switch */
#define HOMING_DIR_Z         STEPPER_DIR_CCW  /* Direction towards Z home switch */

#define GRIPPER_SERVO_OPEN_ANGLE_DEG   20
#define GRIPPER_SERVO_CLOSE_ANGLE_DEG  70

static stepper_motor_t *motor_x = NULL;
static stepper_motor_t *motor_y1 = NULL;
static stepper_motor_t *motor_y2 = NULL;
static stepper_motor_t *motor_z = NULL;

static servo_motor_t *gripper_servo = NULL;

/* Limit switches */
static limit_switch_t *limit_x = NULL;
static limit_switch_t *limit_y = NULL;
static limit_switch_t *limit_z = NULL;

/* Homing state */
static volatile homing_state_t homing_state = HOMING_STATE_IDLE;

K_MSGQ_DEFINE(action_queue, sizeof(planner_action_t), 4,
              _Alignof(planner_action_t));

static volatile bool planner_active = false;
static robot_action_complete_cb_t action_complete_cb = NULL;

/* Limit switch callbacks - called from ISR context */
static void on_limit_x_triggered(limit_switch_t *sw, void *user_data)
{
    LOG_INF("X limit switch triggered - motor stopped, position zeroed");
    stepper_motor_set_position(motor_x, 0);
}

static void on_limit_y_triggered(limit_switch_t *sw, void *user_data)
{
    LOG_INF("Y limit switch triggered - motors stopped, positions zeroed");
    stepper_motor_set_position(motor_y1, 0);
    stepper_motor_set_position(motor_y2, 0);
}

static void on_limit_z_triggered(limit_switch_t *sw, void *user_data)
{
    LOG_INF("Z limit switch triggered - motor stopped, position zeroed");
    stepper_motor_set_position(motor_z, 0);
}

static void motor_move_complete(stepper_motor_t *motor)
{
    LOG_DBG("Motor movement complete");
}

int robot_controller_init(void)
{
    int ret;
    
    ret = stepper_manager_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize stepper manager: %d", ret);
        return ret;
    }
    
    motor_x = stepper_motor_create(
        DEVICE_DT_GET(STEPPER_X_PULSE_PORT), STEPPER_X_PULSE_PIN,
        DEVICE_DT_GET(STEPPER_X_DIR_PORT), STEPPER_X_DIR_PIN,
        DEVICE_DT_GET(STEPPER_X_ENABLE_PORT), STEPPER_X_ENABLE_PIN
    );
    if (!motor_x) {
        LOG_ERR("Failed to create X motor");
        return -ENOMEM;
    }
    
    motor_y1 = stepper_motor_create(
        DEVICE_DT_GET(STEPPER_Y1_PULSE_PORT), STEPPER_Y1_PULSE_PIN,
        DEVICE_DT_GET(STEPPER_Y1_DIR_PORT), STEPPER_Y1_DIR_PIN,
        DEVICE_DT_GET(STEPPER_Y1_ENABLE_PORT), STEPPER_Y1_ENABLE_PIN
    );
    if (!motor_y1) {
        LOG_ERR("Failed to create Y1 motor");
        return -ENOMEM;
    }

    motor_y2 = stepper_motor_create(
        DEVICE_DT_GET(STEPPER_Y2_PULSE_PORT), STEPPER_Y2_PULSE_PIN,
        DEVICE_DT_GET(STEPPER_Y2_DIR_PORT), STEPPER_Y2_DIR_PIN,
        DEVICE_DT_GET(STEPPER_Y2_ENABLE_PORT), STEPPER_Y2_ENABLE_PIN
    );
    if (!motor_y2) {
        LOG_ERR("Failed to create Y2 motor");
        return -ENOMEM;
    }
    
    motor_z = stepper_motor_create(
        DEVICE_DT_GET(STEPPER_Z_PULSE_PORT), STEPPER_Z_PULSE_PIN,
        DEVICE_DT_GET(STEPPER_Z_DIR_PORT), STEPPER_Z_DIR_PIN,
        DEVICE_DT_GET(STEPPER_Z_ENABLE_PORT), STEPPER_Z_ENABLE_PIN
    );
    if (!motor_z) {
        LOG_ERR("Failed to create Z motor");
        return -ENOMEM;
    }
    
    ret = stepper_motor_init(motor_x);
    if (ret < 0) {
        LOG_ERR("Failed to initialize X motor: %d", ret);
        return ret;
    }
    stepper_motor_register_callback(motor_x, motor_move_complete);
    
    ret = stepper_motor_init(motor_y1);
    if (ret < 0) {
        LOG_ERR("Failed to initialize Y1 motor: %d", ret);
        return ret;
    }
    stepper_motor_register_callback(motor_y1, motor_move_complete);

    ret = stepper_motor_init(motor_y2);
    if (ret < 0) {
        LOG_ERR("Failed to initialize Y2 motor: %d", ret);
        return ret;
    }
    stepper_motor_register_callback(motor_y2, motor_move_complete);
    
    ret = stepper_motor_init(motor_z);
    if (ret < 0) {
        LOG_ERR("Failed to initialize Z motor: %d", ret);
        return ret;
    }
    stepper_motor_register_callback(motor_z, motor_move_complete);
    
    stepper_manager_register_motor(STEPPER_ID_X_AXIS, motor_x);
    stepper_manager_register_motor(STEPPER_ID_Y1_AXIS, motor_y1);
    stepper_manager_register_motor(STEPPER_ID_Y2_AXIS, motor_y2);
    stepper_manager_register_motor(STEPPER_ID_Z_AXIS, motor_z);
    
    ret = stepper_manager_enable_all(true);
    if (ret < 0) {
        LOG_ERR("Failed to enable motors: %d", ret);
        return ret;
    }
    
    /* Initialize limit switches */
    ret = limit_switch_init();
    if (ret < 0) {
        LOG_WRN("Failed to initialize limit switches: %d (homing may not work)", ret);
    } else {
        /* Get limit switch instances */
        limit_x = limit_switch_get(LIMIT_SWITCH_X);
        limit_y = limit_switch_get(LIMIT_SWITCH_Y);
        limit_z = limit_switch_get(LIMIT_SWITCH_Z);
        
        /* Attach motors to limit switches for automatic emergency stop */
        if (limit_x) {
            limit_switch_attach_motor(limit_x, motor_x);
            limit_switch_register_callback(limit_x, on_limit_x_triggered, NULL);
            LOG_INF("X motor attached to limit switch");
        }
        
        if (limit_y) {
            /* Y-axis has dual motors sharing one limit switch */
            limit_switch_attach_motor(limit_y, motor_y1);
            limit_switch_attach_motor_secondary(limit_y, motor_y2);
            limit_switch_register_callback(limit_y, on_limit_y_triggered, NULL);
            LOG_INF("Y motors attached to limit switch");
        }
        
        if (limit_z) {
            limit_switch_attach_motor(limit_z, motor_z);
            limit_switch_register_callback(limit_z, on_limit_z_triggered, NULL);
            LOG_INF("Z motor attached to limit switch");
        }
    }
    
    ret = servo_manager_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize servo manager: %d", ret);
        return ret;
    }
    
    gripper_servo = servo_motor_create(DEVICE_DT_GET(GRIPPER_SERVO_GPIO_PORT), GRIPPER_SERVO_GPIO_PIN, GRIPPER_SERVO_GPIO_FLAGS);
    if (!gripper_servo) {
        LOG_ERR("Failed to create gripper servo");
        return -ENOMEM;
    }
    
    ret = servo_motor_init(gripper_servo);
    if (ret < 0) {
        LOG_ERR("Failed to initialize gripper servo: %d", ret);
        return ret;
    }
    
    servo_manager_register_servo(SERVO_ID_1, gripper_servo);
    
    movement_planner_init();

    LOG_INF("Robot controller initialized");
    return 0;
}

int robot_controller_move_to(int32_t x, int32_t y, int32_t z, uint32_t speed_us)
{
    int ret;
    
    if (!motor_x || !motor_y1 || !motor_y2 || !motor_z) {
        return -EINVAL;
    }
    
    int32_t current_x = stepper_motor_get_position(motor_x);
    int32_t current_y = stepper_motor_get_position(motor_y1);
    int32_t current_z = stepper_motor_get_position(motor_z);
    
    int32_t steps_x = x - current_x;
    int32_t steps_y = y - current_y;
    int32_t steps_z = z - current_z;
    
    if (steps_x != 0) {
        ret = stepper_motor_move_steps(motor_x, steps_x, speed_us);
        if (ret < 0) {
            return ret;
        }
    }
    
    if (steps_y != 0) {
        ret = stepper_motor_move_steps_sync(motor_y1, motor_y2, steps_y, speed_us);
        if (ret < 0) {
            return ret;
        }
    }
    
    if (steps_z != 0) {
        ret = stepper_motor_move_steps(motor_z, steps_z, speed_us);
        if (ret < 0) {
            return ret;
        }
    }
    
    LOG_INF("Moving to position X=%d Y=%d Z=%d", x, y, z);
    return 0;
}

int robot_controller_home(void)
{
    /* Legacy function - just zeroes positions without using limit switches */
    if (!motor_x || !motor_y1 || !motor_y2 || !motor_z) {
        return -EINVAL;
    }
    
    stepper_motor_set_position(motor_x, 0);
    stepper_motor_set_position(motor_y1, 0);
    stepper_motor_set_position(motor_y2, 0);
    stepper_motor_set_position(motor_z, 0);
    
    LOG_INF("Position counters zeroed (no physical homing)");
    return 0;
}

int robot_controller_home_axis(char axis)
{
    int ret;
    
    switch (axis) {
    case 'x':
    case 'X':
        if (!motor_x) {
            return -EINVAL;
        }
        if (!limit_x) {
            LOG_ERR("X limit switch not available");
            return -ENODEV;
        }
        limit_switch_clear_triggered(limit_x);
        ret = stepper_motor_start_homing(motor_x, HOMING_DIR_X, HOMING_SPEED_US);
        if (ret == 0) {
            LOG_INF("X-axis homing started");
        }
        return ret;
        
    case 'y':
    case 'Y':
        if (!motor_y1 || !motor_y2) {
            return -EINVAL;
        }
        if (!limit_y) {
            LOG_ERR("Y limit switch not available");
            return -ENODEV;
        }
        limit_switch_clear_triggered(limit_y);
        ret = stepper_motor_start_homing_sync(motor_y1, motor_y2, HOMING_DIR_Y, HOMING_SPEED_US);
        if (ret == 0) {
            LOG_INF("Y-axis homing started");
        }
        return ret;
        
    case 'z':
    case 'Z':
        if (!motor_z) {
            return -EINVAL;
        }
        if (!limit_z) {
            LOG_ERR("Z limit switch not available");
            return -ENODEV;
        }
        limit_switch_clear_triggered(limit_z);
        ret = stepper_motor_start_homing(motor_z, HOMING_DIR_Z, HOMING_SPEED_US);
        if (ret == 0) {
            LOG_INF("Z-axis homing started");
        }
        return ret;
        
    default:
        LOG_ERR("Unknown axis: %c", axis);
        return -EINVAL;
    }
}

int robot_controller_home_all(void)
{
    int ret;
    
    /* Start homing sequence: Z first (safety), then Y, then X */
    homing_state = HOMING_STATE_Z;
    
    ret = robot_controller_home_axis('z');
    if (ret < 0) {
        homing_state = HOMING_STATE_ERROR;
        return ret;
    }
    
    LOG_INF("Home all sequence started (Z -> Y -> X)");
    return 0;
}

homing_state_t robot_controller_get_homing_state(void)
{
    return homing_state;
}

bool robot_controller_is_homing(void)
{
    return (homing_state != HOMING_STATE_IDLE && 
            homing_state != HOMING_STATE_COMPLETE &&
            homing_state != HOMING_STATE_ERROR);
}

bool robot_controller_limit_switch_triggered(char axis)
{
    switch (axis) {
    case 'x':
    case 'X':
        return limit_x ? limit_switch_is_triggered(limit_x) : false;
    case 'y':
    case 'Y':
        return limit_y ? limit_switch_is_triggered(limit_y) : false;
    case 'z':
    case 'Z':
        return limit_z ? limit_switch_is_triggered(limit_z) : false;
    default:
        return false;
    }
}

int robot_controller_gripper_open(void)
{
    if (!gripper_servo) {
        return -EINVAL;
    }

    (void)servo_motor_enable(gripper_servo, true);
    return servo_motor_set_angle(gripper_servo, GRIPPER_SERVO_OPEN_ANGLE_DEG);
}

int robot_controller_gripper_close(void)
{
    if (!gripper_servo) {
        return -EINVAL;
    }

    (void)servo_motor_enable(gripper_servo, true);
    return servo_motor_set_angle(gripper_servo, GRIPPER_SERVO_CLOSE_ANGLE_DEG);
}

int robot_controller_servo_set_angle(uint8_t servo_id, uint16_t angle_degrees)
{
    servo_motor_t *servo = servo_manager_get_servo(servo_id);
    if (!servo) {
        LOG_ERR("Servo %u not found", servo_id);
        return -EINVAL;
    }
    
    return servo_motor_set_angle(servo, angle_degrees);
}

int robot_controller_servo_enable(uint8_t servo_id, bool enable)
{
    servo_motor_t *servo = servo_manager_get_servo(servo_id);
    if (!servo) {
        LOG_ERR("Servo %u not found", servo_id);
        return -EINVAL;
    }
    
    return servo_motor_enable(servo, enable);
}

bool robot_controller_is_busy(void)
{
    /* Busy while a planner action is executing OR any motor is still moving */
    return planner_active || !stepper_manager_all_idle();
}

robot_position_t robot_controller_get_position(void)
{
    robot_position_t pos = {0, 0, 0};
    
    if (motor_x) {
        pos.x = stepper_motor_get_position(motor_x);
    }
    if (motor_y1) {
        pos.y = stepper_motor_get_position(motor_y1);
    }
    if (motor_z) {
        pos.z = stepper_motor_get_position(motor_z);
    }
    
    return pos;
}

void robot_controller_update(void)
{
    stepper_manager_update_all();

    /* Handle homing state machine */
    if (homing_state == HOMING_STATE_Z) {
        /* Check if Z homing complete (limit switch triggered and motor stopped) */
        if (limit_z && limit_switch_was_triggered(limit_z) && !stepper_motor_is_homing(motor_z)) {
            LOG_INF("Z-axis homed, starting Y-axis");
            homing_state = HOMING_STATE_Y;
            if (robot_controller_home_axis('y') < 0) {
                homing_state = HOMING_STATE_ERROR;
            }
        }
    } else if (homing_state == HOMING_STATE_Y) {
        /* Check if Y homing complete */
        if (limit_y && limit_switch_was_triggered(limit_y) && 
            !stepper_motor_is_homing(motor_y1) && !stepper_motor_is_homing(motor_y2)) {
            LOG_INF("Y-axis homed, starting X-axis");
            homing_state = HOMING_STATE_X;
            if (robot_controller_home_axis('x') < 0) {
                homing_state = HOMING_STATE_ERROR;
            }
        }
    } else if (homing_state == HOMING_STATE_X) {
        /* Check if X homing complete */
        if (limit_x && limit_switch_was_triggered(limit_x) && !stepper_motor_is_homing(motor_x)) {
            LOG_INF("X-axis homed - All axes homed successfully!");
            homing_state = HOMING_STATE_COMPLETE;
        }
    }
}

int robot_controller_start_xy_move(int32_t x_abs, int32_t y_abs, uint32_t speed_us)
{
    if (!motor_x || !motor_y1 || !motor_y2) {
        return -EINVAL;
    }

    int32_t steps_x = x_abs - stepper_motor_get_position(motor_x);
    int32_t steps_y = y_abs - stepper_motor_get_position(motor_y1);
    int ret;

    /*
     * Start X and Y in separate (non-blocking) calls before the first
     * stepper_manager_update_all() tick, so both axes step concurrently
     * from the very first update cycle.
     */
    if (steps_x != 0) {
        ret = stepper_motor_move_steps(motor_x, steps_x, speed_us);
        if (ret < 0) {
            return ret;
        }
    }
    if (steps_y != 0) {
        ret = stepper_motor_move_steps_sync(motor_y1, motor_y2, steps_y, speed_us);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int robot_controller_start_z_move(int32_t z_abs, uint32_t speed_us)
{
    if (!motor_z) {
        return -EINVAL;
    }

    int32_t steps_z = z_abs - stepper_motor_get_position(motor_z);
    if (steps_z == 0) {
        return 0;
    }

    return stepper_motor_move_steps(motor_z, steps_z, speed_us);
}

bool robot_controller_is_xy_moving(void)
{
    return stepper_motor_is_moving(motor_x)  ||
           stepper_motor_is_moving(motor_y1) ||
           stepper_motor_is_moving(motor_y2);
}

bool robot_controller_is_z_moving(void)
{
    return stepper_motor_is_moving(motor_z);
}

int robot_controller_enqueue_action(const planner_action_t *action)
{
    if (!action) {
        return -EINVAL;
    }

    int ret = k_msgq_put(&action_queue, action, K_NO_WAIT);
    if (ret < 0) {
        LOG_WRN("Action queue full – action dropped (ret=%d)", ret);
    }
    return ret;
}

void robot_controller_set_action_complete_cb(robot_action_complete_cb_t cb)
{
    action_complete_cb = cb;
}

void robot_controller_task(void)
{
    planner_action_t pending;

    while (1) {
        /*
         * Dequeue and execute the next planned action when the robot is
         * idle and no homing sequence is running.  movement_planner_execute()
         * blocks internally, driving the stepper update loop itself until
         * the action sequence is fully complete.
         */
        if (!robot_controller_is_homing() &&
            k_msgq_get(&action_queue, &pending, K_NO_WAIT) == 0) {

            planner_active = true;
            planner_result_t result = movement_planner_execute(&pending);
            planner_active = false;

            if (action_complete_cb) {
                action_complete_cb(result, &pending);
            }
        }

        robot_controller_update();
        k_sleep(K_USEC(100));
    }
}
