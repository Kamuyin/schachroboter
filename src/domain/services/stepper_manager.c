#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "stepper_manager.h"
#include "limit_switch.h"

LOG_MODULE_REGISTER(stepper_manager, LOG_LEVEL_INF);

static stepper_motor_t *motors[MAX_STEPPER_MOTORS];
static bool initialized = false;

int stepper_manager_init(void)
{
    if (initialized) {
        return 0;
    }
    
    memset(motors, 0, sizeof(motors));
    initialized = true;
    
    LOG_INF("Stepper manager initialized");
    return 0;
}

int stepper_manager_register_motor(stepper_id_t id, stepper_motor_t *motor)
{
    if (!initialized) {
        LOG_ERR("Manager not initialized");
        return -EINVAL;
    }
    
    if (id >= MAX_STEPPER_MOTORS) {
        LOG_ERR("Invalid motor ID: %d", id);
        return -EINVAL;
    }
    
    if (!motor) {
        LOG_ERR("Motor pointer is NULL");
        return -EINVAL;
    }
    
    motors[id] = motor;
    LOG_INF("Motor registered at ID %d", id);
    
    return 0;
}

stepper_motor_t *stepper_manager_get_motor(stepper_id_t id)
{
    if (!initialized || id >= MAX_STEPPER_MOTORS) {
        return NULL;
    }
    
    return motors[id];
}

void stepper_manager_update_all(void)
{
    if (!initialized) {
        return;
    }

    /*
     * Hard safety layer: poll all limit switches on every motion tick.
     * This guarantees emergency-stop behavior even if GPIO interrupts fail.
     */
    limit_switch_safety_poll();

    bool y_pair_handled = false;

    for (int i = 0; i < MAX_STEPPER_MOTORS; i++) {
        if (!motors[i]) {
            continue;
        }

        if (!y_pair_handled && i == STEPPER_ID_Y1_AXIS && motors[STEPPER_ID_Y2_AXIS]) {
            stepper_motor_update_pair(motors[STEPPER_ID_Y1_AXIS], motors[STEPPER_ID_Y2_AXIS]);
            y_pair_handled = true;
            continue;
        }

        if (y_pair_handled && i == STEPPER_ID_Y2_AXIS) {
            continue; /* already updated as a pair */
        }

        stepper_motor_update(motors[i]);
    }
}

int stepper_manager_enable_all(bool enable)
{
    if (!initialized) {
        return -EINVAL;
    }
    
    int ret;
    for (int i = 0; i < MAX_STEPPER_MOTORS; i++) {
        if (motors[i]) {
            ret = stepper_motor_enable(motors[i], enable);
            if (ret < 0) {
                LOG_ERR("Failed to %s motor %d", enable ? "enable" : "disable", i);
                return ret;
            }
        }
    }
    
    return 0;
}

int stepper_manager_stop_all(void)
{
    if (!initialized) {
        return -EINVAL;
    }
    
    int ret;
    for (int i = 0; i < MAX_STEPPER_MOTORS; i++) {
        if (motors[i]) {
            ret = stepper_motor_stop(motors[i]);
            if (ret < 0) {
                LOG_ERR("Failed to stop motor %d", i);
                return ret;
            }
        }
    }
    
    return 0;
}

bool stepper_manager_all_idle(void)
{
    if (!initialized) {
        return true;
    }
    
    for (int i = 0; i < MAX_STEPPER_MOTORS; i++) {
        if (motors[i] && stepper_motor_is_moving(motors[i])) {
            return false;
        }
    }
    
    return true;
}
