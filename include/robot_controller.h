#ifndef ROBOT_CONTROLLER_H
#define ROBOT_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    int32_t x;
    int32_t y;
    int32_t z;
} robot_position_t;

typedef enum {
    HOMING_STATE_IDLE = 0,
    HOMING_STATE_X,
    HOMING_STATE_Y,
    HOMING_STATE_Z,
    HOMING_STATE_COMPLETE,
    HOMING_STATE_ERROR
} homing_state_t;

int robot_controller_init(void);
int robot_controller_move_to(int32_t x, int32_t y, int32_t z, uint32_t speed_us);
int robot_controller_home(void);
int robot_controller_gripper_open(void);
int robot_controller_gripper_close(void);
int robot_controller_servo_set_angle(uint8_t servo_id, uint16_t angle_degrees);
int robot_controller_servo_enable(uint8_t servo_id, bool enable);
bool robot_controller_is_busy(void);
robot_position_t robot_controller_get_position(void);
void robot_controller_update(void);
void robot_controller_task(void);

int robot_controller_home_all(void);

int robot_controller_home_axis(char axis);

homing_state_t robot_controller_get_homing_state(void);

bool robot_controller_is_homing(void);

bool robot_controller_limit_switch_triggered(char axis);

#endif
