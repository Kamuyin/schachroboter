#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    STEPPER_STATE_IDLE = 0,
    STEPPER_STATE_MOVING,
    STEPPER_STATE_HOMING,
    STEPPER_STATE_ERROR
} stepper_state_t;

typedef enum
{
    STEPPER_DIR_CW = 0,
    STEPPER_DIR_CCW = 1
} stepper_direction_t;

typedef struct stepper_motor stepper_motor_t;

typedef void (*stepper_move_complete_callback_t)(stepper_motor_t *motor);

stepper_motor_t *stepper_motor_create(const struct device *pulse_port, uint32_t pulse_pin,
                                      const struct device *dir_port, uint32_t dir_pin,
                                      const struct device *enable_port, uint32_t enable_pin);

int stepper_motor_init(stepper_motor_t *motor);
int stepper_motor_enable(stepper_motor_t *motor, bool enable);
int stepper_motor_move_steps(stepper_motor_t *motor, int32_t steps, uint32_t step_delay_us);
int stepper_motor_move_steps_sync(stepper_motor_t *motor_a, stepper_motor_t *motor_b,
                                  int32_t steps, uint32_t step_delay_us);
int stepper_motor_stop(stepper_motor_t *motor);
bool stepper_motor_is_moving(const stepper_motor_t *motor);
int32_t stepper_motor_get_position(const stepper_motor_t *motor);
int stepper_motor_set_position(stepper_motor_t *motor, int32_t position);
stepper_state_t stepper_motor_get_state(const stepper_motor_t *motor);
void stepper_motor_register_callback(stepper_motor_t *motor, stepper_move_complete_callback_t callback);
void stepper_motor_update(stepper_motor_t *motor);
void stepper_motor_update_pair(stepper_motor_t *motor_a, stepper_motor_t *motor_b);
void stepper_motor_set_direction_inverted(stepper_motor_t *motor, bool inverted);

/**
 * @brief Emergency stop - immediately halts motor, discards remaining steps
 * @note Safe to call from ISR context (limit switch interrupt)
 * @param motor Pointer to stepper motor
 */
void stepper_motor_emergency_stop(stepper_motor_t *motor);

/**
 * @brief Start homing movement (continuous move until limit switch triggers)
 * @param motor Pointer to stepper motor
 * @param direction Direction to move (towards home position)
 * @param step_delay_us Speed in microseconds per step
 * @return 0 on success, negative errno on failure
 */
int stepper_motor_start_homing(stepper_motor_t *motor, stepper_direction_t direction, uint32_t step_delay_us);

/**
 * @brief Start synchronized homing for dual motors (Y-axis)
 * @param motor_a First motor
 * @param motor_b Second motor
 * @param direction Direction to move (towards home position)
 * @param step_delay_us Speed in microseconds per step
 * @return 0 on success, negative errno on failure
 */
int stepper_motor_start_homing_sync(stepper_motor_t *motor_a, stepper_motor_t *motor_b,
                                    stepper_direction_t direction, uint32_t step_delay_us);

/**
 * @brief Check if motor is in homing state
 * @param motor Pointer to stepper motor
 * @return true if homing, false otherwise
 */
bool stepper_motor_is_homing(const stepper_motor_t *motor);

#endif
