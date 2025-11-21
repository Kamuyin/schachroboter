#ifndef STEPPER_MANAGER_H
#define STEPPER_MANAGER_H

#include "stepper_motor.h"

#define MAX_STEPPER_MOTORS 8

typedef enum {
    STEPPER_ID_X_AXIS = 0,
    STEPPER_ID_Y_AXIS,
    STEPPER_ID_Z_AXIS,
    STEPPER_ID_GRIPPER,
    STEPPER_ID_MAX
} stepper_id_t;

int stepper_manager_init(void);
int stepper_manager_register_motor(stepper_id_t id, stepper_motor_t *motor);
stepper_motor_t *stepper_manager_get_motor(stepper_id_t id);
void stepper_manager_update_all(void);
int stepper_manager_enable_all(bool enable);
int stepper_manager_stop_all(void);
bool stepper_manager_all_idle(void);

#endif
