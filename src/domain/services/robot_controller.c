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

LOG_MODULE_REGISTER(robot_controller, LOG_LEVEL_INF);

static stepper_motor_t *motor_x = NULL;
static stepper_motor_t *motor_y = NULL;
static stepper_motor_t *motor_z = NULL;
static stepper_motor_t *motor_gripper = NULL;

static servo_motor_t *servo_1 = NULL;
static servo_motor_t *servo_2 = NULL;
static servo_motor_t *servo_3 = NULL;
static servo_motor_t *servo_4 = NULL;

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
    
    motor_y = stepper_motor_create(
        DEVICE_DT_GET(STEPPER_Y_PULSE_PORT), STEPPER_Y_PULSE_PIN,
        DEVICE_DT_GET(STEPPER_Y_DIR_PORT), STEPPER_Y_DIR_PIN,
        DEVICE_DT_GET(STEPPER_Y_ENABLE_PORT), STEPPER_Y_ENABLE_PIN
    );
    if (!motor_y) {
        LOG_ERR("Failed to create Y motor");
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
    
    ret = stepper_motor_init(motor_y);
    if (ret < 0) {
        LOG_ERR("Failed to initialize Y motor: %d", ret);
        return ret;
    }
    stepper_motor_register_callback(motor_y, motor_move_complete);
    
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
    stepper_manager_register_motor(STEPPER_ID_Y_AXIS, motor_y);
    stepper_manager_register_motor(STEPPER_ID_Z_AXIS, motor_z);
    stepper_manager_register_motor(STEPPER_ID_GRIPPER, motor_gripper);
    
    ret = stepper_manager_enable_all(true);
    if (ret < 0) {
        LOG_ERR("Failed to enable motors: %d", ret);
        return ret;
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
    
    if (!motor_x || !motor_y || !motor_z) {
        return -EINVAL;
    }
    
    int32_t current_x = stepper_motor_get_position(motor_x);
    int32_t current_y = stepper_motor_get_position(motor_y);
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
        ret = stepper_motor_move_steps(motor_y, steps_y, speed_us);
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
    if (!motor_x || !motor_y || !motor_z) {
        return -EINVAL;
    }
    
    stepper_motor_set_position(motor_x, 0);
    stepper_motor_set_position(motor_y, 0);
    stepper_motor_set_position(motor_z, 0);
    
    LOG_INF("Homing complete");
    return 0;
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
    if (motor_y) {
        pos.y = stepper_motor_get_position(motor_y);
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
}

void robot_controller_task(void)
{
    while (1) {
        robot_controller_update();
        k_sleep(K_USEC(100));
    }
}
