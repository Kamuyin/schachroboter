#ifndef SERVO_MOTOR_H
#define SERVO_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/drivers/gpio.h>

#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500
#define SERVO_PWM_PERIOD_US 20000

typedef struct servo_motor servo_motor_t;

servo_motor_t *servo_motor_create(const struct device *gpio_port, uint32_t gpio_pin, gpio_dt_flags_t gpio_flags);
int servo_motor_init(servo_motor_t *servo);
int servo_motor_set_angle(servo_motor_t *servo, uint16_t angle_degrees);
int servo_motor_set_pulse_width(servo_motor_t *servo, uint32_t pulse_us);
uint16_t servo_motor_get_angle(const servo_motor_t *servo);
int servo_motor_enable(servo_motor_t *servo, bool enable);
bool servo_motor_is_enabled(const servo_motor_t *servo);
void servo_motor_update(servo_motor_t *servo);

#endif
