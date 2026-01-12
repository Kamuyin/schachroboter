#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>

/**
 * @file stepper_motor.h
 * @brief Hardware timer-based PWM stepper motor driver
 * 
 * This driver uses hardware PWM for pulse generation, providing:
 * - Deterministic, jitter-free timing independent of CPU load
 * - Zero CPU overhead during motion
 * - Higher maximum step rates (10+ kHz vs ~1 kHz software)
 * - Improved motion smoothness and precision
 */

typedef enum {
    STEPPER_STATE_IDLE = 0,
    STEPPER_STATE_MOVING,
    STEPPER_STATE_HOMING,
    STEPPER_STATE_ERROR
} stepper_state_t;

typedef enum {
    STEPPER_DIR_CW = 0,
    STEPPER_DIR_CCW = 1
} stepper_direction_t;

typedef struct stepper_motor stepper_motor_t;

typedef void (*stepper_move_complete_callback_t)(stepper_motor_t *motor);

typedef struct {
    const struct device *pwm_dev;   // PWM device
    uint32_t pwm_channel;           // PWM channel
    uint32_t pwm_flags;             // PWM flags (polarity)
    const struct device *dir_port;  // Direction GPIO port
    uint32_t dir_pin;               // Direction GPIO pin
    const struct device *enable_port; // Enable GPIO port (optional)
    uint32_t enable_pin;            // Enable GPIO pin (optional)
    bool has_enable;                // Whether enable GPIO is present
    bool dir_inverted;              // Direction inversion flag
    uint32_t max_speed_hz;          // Maximum step rate in Hz
} stepper_motor_config_t;

stepper_motor_t *stepper_motor_create_from_config(const stepper_motor_config_t *config);

stepper_motor_t *stepper_motor_create_from_dt(const char *node_label);

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

void stepper_motor_register_callback(stepper_motor_t *motor, 
                                     stepper_move_complete_callback_t callback);

void stepper_motor_update(stepper_motor_t *motor);

void stepper_motor_update_pair(stepper_motor_t *motor_a, stepper_motor_t *motor_b);

void stepper_motor_set_direction_inverted(stepper_motor_t *motor, bool inverted);

void stepper_motor_emergency_stop(stepper_motor_t *motor);

int stepper_motor_start_homing(stepper_motor_t *motor, 
                               stepper_direction_t direction, 
                               uint32_t step_delay_us);

int stepper_motor_start_homing_sync(stepper_motor_t *motor_a, 
                                    stepper_motor_t *motor_b,
                                    stepper_direction_t direction, 
                                    uint32_t step_delay_us);

bool stepper_motor_is_homing(const stepper_motor_t *motor);

const struct device *stepper_motor_get_pwm_dev(const stepper_motor_t *motor);

uint32_t stepper_motor_get_pwm_channel(const stepper_motor_t *motor);

#endif /* STEPPER_MOTOR_H */
