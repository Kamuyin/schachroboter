#ifndef SERVO_MANAGER_H
#define SERVO_MANAGER_H

#include "servo_motor.h"

#define MAX_SERVO_MOTORS 8

typedef enum
{
    SERVO_ID_1 = 0,
    SERVO_ID_2,
    SERVO_ID_3,
    SERVO_ID_4,
    SERVO_ID_MAX
} servo_id_t;

int servo_manager_init(void);
int servo_manager_register_servo(servo_id_t id, servo_motor_t *servo);
servo_motor_t *servo_manager_get_servo(servo_id_t id);
int servo_manager_enable_all(bool enable);
int servo_manager_set_all_angle(uint16_t angle_degrees);

#endif
