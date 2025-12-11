#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "servo_manager.h"

LOG_MODULE_REGISTER(servo_manager, LOG_LEVEL_INF);

static servo_motor_t *servos[MAX_SERVO_MOTORS];
static bool initialized = false;

int servo_manager_init(void)
{
    if (initialized) {
        return 0;
    }

    for (int i = 0; i < MAX_SERVO_MOTORS; i++) {
        servos[i] = NULL;
    }

    initialized = true;
    LOG_INF("Servo manager initialized");
    return 0;
}

int servo_manager_register_servo(servo_id_t id, servo_motor_t *servo)
{
    if (!initialized) {
        LOG_ERR("Servo manager not initialized");
        return -EINVAL;
    }

    if (id >= MAX_SERVO_MOTORS || !servo) {
        return -EINVAL;
    }

    servos[id] = servo;
    LOG_INF("Registered servo %d", id);
    return 0;
}

servo_motor_t *servo_manager_get_servo(servo_id_t id)
{
    if (!initialized || id >= MAX_SERVO_MOTORS) {
        return NULL;
    }

    return servos[id];
}

int servo_manager_enable_all(bool enable)
{
    if (!initialized) {
        return -EINVAL;
    }

    int ret;
    for (int i = 0; i < MAX_SERVO_MOTORS; i++) {
        if (servos[i]) {
            ret = servo_motor_enable(servos[i], enable);
            if (ret < 0) {
                LOG_ERR("Failed to %s servo %d: %d", 
                        enable ? "enable" : "disable", i, ret);
                return ret;
            }
        }
    }

    return 0;
}

int servo_manager_set_all_angle(uint16_t angle_degrees)
{
    if (!initialized) {
        return -EINVAL;
    }

    int ret;
    for (int i = 0; i < MAX_SERVO_MOTORS; i++) {
        if (servos[i]) {
            ret = servo_motor_set_angle(servos[i], angle_degrees);
            if (ret < 0) {
                LOG_ERR("Failed to set angle for servo %d: %d", i, ret);
                return ret;
            }
        }
    }

    return 0;
}
