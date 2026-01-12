#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>
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
	bool dir_inverted;
    
	stepper_move_complete_callback_t callback;
};

static inline uint64_t now_us(void)
{
	return k_cyc_to_us_floor64(k_cycle_get_64());
}

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
	motor->dir_inverted = false;
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
	motor->next_step_time = now_us();
    
	motor->direction = (steps > 0) ? STEPPER_DIR_CW : STEPPER_DIR_CCW;
	gpio_pin_set(motor->dir_port, motor->dir_pin, motor->direction ^ motor->dir_inverted);
    
	return 0;
}

int stepper_motor_move_steps_sync(stepper_motor_t *motor_a, stepper_motor_t *motor_b,
								  int32_t steps, uint32_t step_delay_us)
{
	if (!motor_a || !motor_b) {
		return -EINVAL;
	}

	if (!motor_a->enabled || !motor_b->enabled) {
		LOG_WRN("Cannot move motors while disabled (Y dual)");
		return -EACCES;
	}

	if (steps == 0) {
		return 0;
	}

	uint64_t now = now_us();
	stepper_direction_t dir = (steps > 0) ? STEPPER_DIR_CW : STEPPER_DIR_CCW;

	motor_a->target_position = motor_a->current_position + steps;
	motor_b->target_position = motor_b->current_position + steps;

	motor_a->step_delay_us = step_delay_us;
	motor_b->step_delay_us = step_delay_us;

	motor_a->state = STEPPER_STATE_MOVING;
	motor_b->state = STEPPER_STATE_MOVING;

	motor_a->next_step_time = now;
	motor_b->next_step_time = now;

	motor_a->direction = dir;
	motor_b->direction = dir;

	gpio_pin_set(motor_a->dir_port, motor_a->dir_pin, dir ^ motor_a->dir_inverted);
	gpio_pin_set(motor_b->dir_port, motor_b->dir_pin, dir ^ motor_b->dir_inverted);

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

void stepper_motor_set_direction_inverted(stepper_motor_t *motor, bool inverted)
{
	if (motor) {
		motor->dir_inverted = inverted;
	}
}

void stepper_motor_update(stepper_motor_t *motor)
{
	if (!motor || (motor->state != STEPPER_STATE_MOVING && motor->state != STEPPER_STATE_HOMING)) {
		return;
	}
    
	if (motor->state == STEPPER_STATE_MOVING && 
	    motor->current_position == motor->target_position) {
		motor->state = STEPPER_STATE_IDLE;
		if (motor->callback) {
			motor->callback(motor);
		}
		return;
	}
    
	
	uint64_t now = now_us();
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

void stepper_motor_update_pair(stepper_motor_t *motor_a, stepper_motor_t *motor_b)
{
	if (!motor_a && !motor_b) {
		return;
	}

	if (!motor_a || !motor_b) {
		stepper_motor_update(motor_a ? motor_a : motor_b);
		return;
	}

	bool both_active = ((motor_a->state == STEPPER_STATE_MOVING || motor_a->state == STEPPER_STATE_HOMING) &&
	                    (motor_b->state == STEPPER_STATE_MOVING || motor_b->state == STEPPER_STATE_HOMING));

	if (both_active &&
		motor_a->current_position == motor_b->current_position &&
		motor_a->step_delay_us == motor_b->step_delay_us) {

		if (motor_a->state == STEPPER_STATE_MOVING && 
		    motor_a->current_position == motor_a->target_position) {
			motor_a->state = STEPPER_STATE_IDLE;
			motor_b->state = STEPPER_STATE_IDLE;
			if (motor_a->callback) motor_a->callback(motor_a);
			if (motor_b->callback && motor_b->callback != motor_a->callback) motor_b->callback(motor_b);
			return;
		}


		uint64_t now = now_us();
		if (now < motor_a->next_step_time) {
			return;
		}

		gpio_pin_set(motor_a->pulse_port, motor_a->pulse_pin, 1);
		gpio_pin_set(motor_b->pulse_port, motor_b->pulse_pin, 1);
		k_busy_wait(5);
		gpio_pin_set(motor_a->pulse_port, motor_a->pulse_pin, 0);
		gpio_pin_set(motor_b->pulse_port, motor_b->pulse_pin, 0);

		if (motor_a->direction == STEPPER_DIR_CW) {
			motor_a->current_position++;
			motor_b->current_position++;
		} else {
			motor_a->current_position--;
			motor_b->current_position--;
		}

		uint64_t next = now + motor_a->step_delay_us;
		motor_a->next_step_time = next;
		motor_b->next_step_time = next;
		return;
	}

	stepper_motor_update(motor_a);
	stepper_motor_update(motor_b);
}

void stepper_motor_emergency_stop(stepper_motor_t *motor)
{
	if (!motor) {
		return;
	}
	
	//Cal ISR
	motor->target_position = motor->current_position;
	motor->state = STEPPER_STATE_IDLE;
	
}

int stepper_motor_start_homing(stepper_motor_t *motor, stepper_direction_t direction, uint32_t step_delay_us)
{
	if (!motor) {
		return -EINVAL;
	}
	
	if (!motor->enabled) {
		LOG_WRN("Cannot home motor while disabled");
		return -EACCES;
	}
	
	motor->direction = direction;
	motor->step_delay_us = step_delay_us;
	motor->state = STEPPER_STATE_HOMING;
	motor->next_step_time = now_us();
	
	gpio_pin_set(motor->dir_port, motor->dir_pin, direction ^ motor->dir_inverted);
	
	LOG_INF("Motor homing started (dir=%d, speed=%u us)", direction, step_delay_us);
	return 0;
}

int stepper_motor_start_homing_sync(stepper_motor_t *motor_a, stepper_motor_t *motor_b,
                                    stepper_direction_t direction, uint32_t step_delay_us)
{
	if (!motor_a || !motor_b) {
		return -EINVAL;
	}
	
	if (!motor_a->enabled || !motor_b->enabled) {
		LOG_WRN("Cannot home motors while disabled (Y dual)");
		return -EACCES;
	}
	
	uint64_t now = now_us();
	
	motor_a->direction = direction;
	motor_b->direction = direction;
	
	motor_a->step_delay_us = step_delay_us;
	motor_b->step_delay_us = step_delay_us;
	
	motor_a->state = STEPPER_STATE_HOMING;
	motor_b->state = STEPPER_STATE_HOMING;
	
	motor_a->next_step_time = now;
	motor_b->next_step_time = now;
	
	/* Set direction pins */
	gpio_pin_set(motor_a->dir_port, motor_a->dir_pin, direction ^ motor_a->dir_inverted);
	gpio_pin_set(motor_b->dir_port, motor_b->dir_pin, direction ^ motor_b->dir_inverted);
	
	LOG_INF("Y-axis homing started (dir=%d, speed=%u us)", direction, step_delay_us);
	return 0;
}

bool stepper_motor_is_homing(const stepper_motor_t *motor)
{
	return motor && motor->state == STEPPER_STATE_HOMING;
}
