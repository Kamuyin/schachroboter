#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "stepper_motor.h"

LOG_MODULE_REGISTER(stepper_motor, LOG_LEVEL_INF);

struct stepper_motor {
    const struct device *pulse_port;
    uint32_t pulse_pin;
    const struct device *dir_port;
    uint32_t dir_pin;
    const struct device *enable_port;
    uint32_t enable_pin;
    
    int32_t current_position;
    int32_t target_position;
    uint32_t step_delay_us;
    uint64_t next_step_time;
    
    stepper_state_t state;
    stepper_direction_t direction;
    bool enabled;
    
    stepper_move_complete_callback_t callback;
};

stepper_motor_t *stepper_motor_create(const struct device *pulse_port, uint32_t pulse_pin,
                                      const struct device *dir_port, uint32_t dir_pin,
                                      const struct device *enable_port, uint32_t enable_pin)
{
    stepper_motor_t *motor = k_malloc(sizeof(stepper_motor_t));
    if (!motor) {
        return NULL;
    }
    
    motor->pulse_port = pulse_port;
    motor->pulse_pin = pulse_pin;
    motor->dir_port = dir_port;
    motor->dir_pin = dir_pin;
    motor->enable_port = enable_port;
    motor->enable_pin = enable_pin;
    
    motor->current_position = 0;
    motor->target_position = 0;
    motor->step_delay_us = 1000;
    motor->next_step_time = 0;
    
    motor->state = STEPPER_STATE_IDLE;
    motor->direction = STEPPER_DIR_CW;
    motor->enabled = false;
    motor->callback = NULL;
    
    return motor;
}

int stepper_motor_init(stepper_motor_t *motor)
{
    int ret;
    
    if (!motor) {
        return -EINVAL;
    }
    
    if (!device_is_ready(motor->pulse_port)) {
        LOG_ERR("Pulse GPIO port not ready");
        return -ENODEV;
    }
    
    if (!device_is_ready(motor->dir_port)) {
        LOG_ERR("Direction GPIO port not ready");
        return -ENODEV;
    }
    
    if (!device_is_ready(motor->enable_port)) {
        LOG_ERR("Enable GPIO port not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure(motor->pulse_port, motor->pulse_pin, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure pulse pin: %d", ret);
        return ret;
    }
    
    ret = gpio_pin_configure(motor->dir_port, motor->dir_pin, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure direction pin: %d", ret);
        return ret;
    }
    
    ret = gpio_pin_configure(motor->enable_port, motor->enable_pin, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure enable pin: %d", ret);
        return ret;
    }
    
    motor->enabled = false;
    motor->state = STEPPER_STATE_IDLE;
    
    return 0;
}

int stepper_motor_enable(stepper_motor_t *motor, bool enable)
{
    int ret;
    
    if (!motor) {
        return -EINVAL;
    }
    
    ret = gpio_pin_set(motor->enable_port, motor->enable_pin, enable ? 0 : 1);
    if (ret < 0) {
        LOG_ERR("Failed to set enable pin: %d", ret);
        return ret;
    }
    
    motor->enabled = enable;
    
    if (!enable && motor->state == STEPPER_STATE_MOVING) {
        motor->state = STEPPER_STATE_IDLE;
    }
    
    return 0;
}

int stepper_motor_move_steps(stepper_motor_t *motor, int32_t steps, uint32_t step_delay_us)
{
    if (!motor) {
        return -EINVAL;
    }
    
    if (!motor->enabled) {
        LOG_WRN("Cannot move motor while disabled");
        return -EACCES;
    }
    
    if (steps == 0) {
        return 0;
    }
    
    motor->target_position = motor->current_position + steps;
    motor->step_delay_us = step_delay_us;
    motor->state = STEPPER_STATE_MOVING;
    motor->next_step_time = k_uptime_get() * 1000;
    
    motor->direction = (steps > 0) ? STEPPER_DIR_CW : STEPPER_DIR_CCW;
    gpio_pin_set(motor->dir_port, motor->dir_pin, motor->direction);
    
    return 0;
}

int stepper_motor_stop(stepper_motor_t *motor)
{
    if (!motor) {
        return -EINVAL;
    }
    
    motor->target_position = motor->current_position;
    motor->state = STEPPER_STATE_IDLE;
    
    return 0;
}

bool stepper_motor_is_moving(const stepper_motor_t *motor)
{
    return motor && motor->state == STEPPER_STATE_MOVING;
}

int32_t stepper_motor_get_position(const stepper_motor_t *motor)
{
    return motor ? motor->current_position : 0;
}

int stepper_motor_set_position(stepper_motor_t *motor, int32_t position)
{
    if (!motor) {
        return -EINVAL;
    }
    
    motor->current_position = position;
    return 0;
}

stepper_state_t stepper_motor_get_state(const stepper_motor_t *motor)
{
    return motor ? motor->state : STEPPER_STATE_ERROR;
}

void stepper_motor_register_callback(stepper_motor_t *motor, stepper_move_complete_callback_t callback)
{
    if (motor) {
        motor->callback = callback;
    }
}

void stepper_motor_update(stepper_motor_t *motor)
{
    if (!motor || motor->state != STEPPER_STATE_MOVING) {
        return;
    }
    
    if (motor->current_position == motor->target_position) {
        motor->state = STEPPER_STATE_IDLE;
        if (motor->callback) {
            motor->callback(motor);
        }
        return;
    }
    
    uint64_t now = k_uptime_get() * 1000;
    if (now < motor->next_step_time) {
        return;
    }
    
    gpio_pin_set(motor->pulse_port, motor->pulse_pin, 1);
    k_busy_wait(5);
    gpio_pin_set(motor->pulse_port, motor->pulse_pin, 0);
    
    if (motor->direction == STEPPER_DIR_CW) {
        motor->current_position++;
    } else {
        motor->current_position--;
    }
    
    motor->next_step_time = now + motor->step_delay_us;
}
