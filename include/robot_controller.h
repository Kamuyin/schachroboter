#ifndef ROBOT_CONTROLLER_H
#define ROBOT_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} robot_position_t;

int robot_controller_init(void);
int robot_controller_move_to(int32_t x, int32_t y, int32_t z, uint32_t speed_us);
int robot_controller_home(void);
int robot_controller_gripper_open(void);
int robot_controller_gripper_close(void);
bool robot_controller_is_busy(void);
robot_position_t robot_controller_get_position(void);
void robot_controller_update(void);
void robot_controller_task(void);

#endif
