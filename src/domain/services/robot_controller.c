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

LOG_MODULE_REGISTER(robot_controller, LOG_LEVEL_INF);

/* Homing configuration */
#define HOMING_SPEED_US      2000  /* Slower speed for homing (safety) */
#define HOMING_DIR_X         STEPPER_DIR_CCW  /* Direction towards X home switch */
#define HOMING_DIR_Y         STEPPER_DIR_CCW  /* Direction towards Y home switch */
#define HOMING_DIR_Z         STEPPER_DIR_CCW  /* Direction towards Z home switch */

static stepper_motor_t *motor_x = NULL;
static stepper_motor_t *motor_y1 = NULL;
static stepper_motor_t *motor_y2 = NULL;
static stepper_motor_t *motor_z = NULL;
static stepper_motor_t *motor_gripper = NULL;

static servo_motor_t *servo_1 = NULL;
static servo_motor_t *servo_2 = NULL;
static servo_motor_t *servo_3 = NULL;
static servo_motor_t *servo_4 = NULL;

/* Limit switches */
static limit_switch_t *limit_x = NULL;
static limit_switch_t *limit_y = NULL;
static limit_switch_t *limit_z = NULL;

/* Homing state */
static volatile homing_state_t homing_state = HOMING_STATE_IDLE;

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
    
    motor_gripper = stepper_motor_create(
        DEVICE_DT_GET(STEPPER_GRIPPER_PULSE_PORT), STEPPER_GRIPPER_PULSE_PIN,
        DEVICE_DT_GET(STEPPER_GRIPPER_DIR_PORT), STEPPER_GRIPPER_DIR_PIN,
        DEVICE_DT_GET(STEPPER_GRIPPER_ENABLE_PORT), STEPPER_GRIPPER_ENABLE_PIN
    );
    if (!motor_gripper) {
        LOG_ERR("Failed to create gripper motor");
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
    
    ret = stepper_motor_init(motor_gripper);
    if (ret < 0) {
        LOG_ERR("Failed to initialize gripper motor: %d", ret);
        return ret;
    }
    stepper_motor_register_callback(motor_gripper, motor_move_complete);
    
    stepper_manager_register_motor(STEPPER_ID_X_AXIS, motor_x);
    stepper_manager_register_motor(STEPPER_ID_Y1_AXIS, motor_y1);
    stepper_manager_register_motor(STEPPER_ID_Y2_AXIS, motor_y2);
    stepper_manager_register_motor(STEPPER_ID_Z_AXIS, motor_z);
    stepper_manager_register_motor(STEPPER_ID_GRIPPER, motor_gripper);
    
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
    
    servo_1 = servo_motor_create(DEVICE_DT_GET(SERVO_1_GPIO_PORT), SERVO_1_GPIO_PIN);
    if (servo_1) {
        ret = servo_motor_init(servo_1);
        if (ret == 0) {
            servo_manager_register_servo(SERVO_ID_1, servo_1);
        } else {
            LOG_WRN("Failed to initialize servo 1: %d", ret);
        }
    }
    
    servo_2 = servo_motor_create(DEVICE_DT_GET(SERVO_2_GPIO_PORT), SERVO_2_GPIO_PIN);
    if (servo_2) {
        ret = servo_motor_init(servo_2);
        if (ret == 0) {
            servo_manager_register_servo(SERVO_ID_2, servo_2);
        } else {
            LOG_WRN("Failed to initialize servo 2: %d", ret);
        }
    }
    
    servo_3 = servo_motor_create(DEVICE_DT_GET(SERVO_3_GPIO_PORT), SERVO_3_GPIO_PIN);
    if (servo_3) {
        ret = servo_motor_init(servo_3);
        if (ret == 0) {
            servo_manager_register_servo(SERVO_ID_3, servo_3);
        } else {
            LOG_WRN("Failed to initialize servo 3: %d", ret);
        }
    }
    
    servo_4 = servo_motor_create(DEVICE_DT_GET(SERVO_4_GPIO_PORT), SERVO_4_GPIO_PIN);
    if (servo_4) {
        ret = servo_motor_init(servo_4);
        if (ret == 0) {
            servo_manager_register_servo(SERVO_ID_4, servo_4);
        } else {
            LOG_WRN("Failed to initialize servo 4: %d", ret);
        }
    }
    
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
        limit_switch_enable_interrupt(limit_x, true);  /* Enable interrupt for homing */
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
        limit_switch_enable_interrupt(limit_y, true);  /* Enable interrupt for homing */
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
        limit_switch_enable_interrupt(limit_z, true);  /* Enable interrupt for homing */
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
    if (!motor_gripper) {
        return -EINVAL;
    }
    
    return stepper_motor_move_steps(motor_gripper, -200, STEPPER_SLOW_SPEED_US);
}

int robot_controller_gripper_close(void)
{
    if (!motor_gripper) {
        return -EINVAL;
    }
    
    return stepper_motor_move_steps(motor_gripper, 200, STEPPER_SLOW_SPEED_US);
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
    return !stepper_manager_all_idle();
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
    
    if (servo_1) servo_motor_update(servo_1);
    if (servo_2) servo_motor_update(servo_2);
    if (servo_3) servo_motor_update(servo_3);
    if (servo_4) servo_motor_update(servo_4);
    
    /* Handle homing state machine */
    if (homing_state == HOMING_STATE_Z) {
        /* Check if Z homing complete (limit switch triggered and motor stopped) */
        if (limit_z && limit_switch_was_triggered(limit_z) && !stepper_motor_is_homing(motor_z)) {
            limit_switch_enable_interrupt(limit_z, false);  /* Disable interrupt after homing */
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
            limit_switch_enable_interrupt(limit_y, false);  /* Disable interrupt after homing */
            LOG_INF("Y-axis homed, starting X-axis");
            homing_state = HOMING_STATE_X;
            if (robot_controller_home_axis('x') < 0) {
                homing_state = HOMING_STATE_ERROR;
            }
        }
    } else if (homing_state == HOMING_STATE_X) {
        /* Check if X homing complete */
        if (limit_x && limit_switch_was_triggered(limit_x) && !stepper_motor_is_homing(motor_x)) {
            limit_switch_enable_interrupt(limit_x, false);  /* Disable interrupt after homing */
            LOG_INF("X-axis homed - All axes homed successfully!");
            homing_state = HOMING_STATE_COMPLETE;
        }
    }
}

void robot_controller_task(void)
{
    while (1) {
        robot_controller_update();
        k_sleep(K_USEC(100));
    }
}
