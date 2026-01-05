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

/**
 * @brief Homing state for tracking async homing operations
 */
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

/**
 * @brief Start homing sequence for all axes (X, Y, Z)
 * 
 * This initiates an asynchronous homing sequence. Motors move towards
 * their respective limit switches. When a switch triggers, the motor
 * stops immediately and its position is set to 0.
 * 
 * @return 0 on success, negative errno on failure
 */
int robot_controller_home_all(void);

/**
 * @brief Home a specific axis
 * @param axis 'x', 'y', or 'z'
 * @return 0 on success, negative errno on failure
 */
int robot_controller_home_axis(char axis);

/**
 * @brief Get current homing state
 * @return Current homing state
 */
homing_state_t robot_controller_get_homing_state(void);

/**
 * @brief Check if any axis is currently homing
 * @return true if homing in progress
 */
bool robot_controller_is_homing(void);

/**
 * @brief Check if a specific limit switch is triggered
 * @param axis 'x', 'y', or 'z'
 * @return true if triggered, false otherwise
 */
bool robot_controller_limit_switch_triggered(char axis);

#endif
