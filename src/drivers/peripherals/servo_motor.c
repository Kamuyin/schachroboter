#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "servo_motor.h"

LOG_MODULE_REGISTER(servo_motor, LOG_LEVEL_INF);

struct servo_motor {
    const struct device *gpio_port;
    uint32_t gpio_pin;
    uint16_t current_angle;
    uint32_t current_pulse_us;
    bool enabled;
    uint64_t pulse_end_time;
};

servo_motor_t *servo_motor_create(const struct device *gpio_port, uint32_t gpio_pin)
{
    if (!gpio_port) {
        return NULL;
    }

    servo_motor_t *servo = k_malloc(sizeof(servo_motor_t));
    if (!servo) {
        return NULL;
    }

    servo->gpio_port = gpio_port;
    servo->gpio_pin = gpio_pin;
    servo->current_angle = 90;
    servo->current_pulse_us = (SERVO_MIN_PULSE_US + SERVO_MAX_PULSE_US) / 2;
    servo->enabled = false;
    servo->pulse_end_time = 0;

    return servo;
}

int servo_motor_init(servo_motor_t *servo)
{
    if (!servo) {
        return -EINVAL;
    }

    if (!device_is_ready(servo->gpio_port)) {
        LOG_ERR("GPIO port not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure(servo->gpio_port, servo->gpio_pin, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure GPIO pin: %d", ret);
        return ret;
    }

    servo->enabled = false;
    LOG_INF("Servo motor initialized on GPIO pin %u", servo->gpio_pin);
    return 0;
}

int servo_motor_set_angle(servo_motor_t *servo, uint16_t angle_degrees)
{
    if (!servo) {
        return -EINVAL;
    }

    if (angle_degrees > SERVO_MAX_ANGLE) {
        angle_degrees = SERVO_MAX_ANGLE;
    }

    uint32_t pulse_us = SERVO_MIN_PULSE_US + 
                        ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * angle_degrees) / SERVO_MAX_ANGLE;

    return servo_motor_set_pulse_width(servo, pulse_us);
}

int servo_motor_set_pulse_width(servo_motor_t *servo, uint32_t pulse_us)
{
    if (!servo) {
        return -EINVAL;
    }

    if (pulse_us < SERVO_MIN_PULSE_US) {
        pulse_us = SERVO_MIN_PULSE_US;
    }
    if (pulse_us > SERVO_MAX_PULSE_US) {
        pulse_us = SERVO_MAX_PULSE_US;
    }

    servo->current_pulse_us = pulse_us;
    servo->current_angle = ((pulse_us - SERVO_MIN_PULSE_US) * SERVO_MAX_ANGLE) / 
                           (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US);

    if (servo->enabled) {
        servo->pulse_end_time = k_uptime_ticks() + k_us_to_ticks_ceil32(pulse_us);
        gpio_pin_set(servo->gpio_port, servo->gpio_pin, 1);
    }

    return 0;
}

uint16_t servo_motor_get_angle(const servo_motor_t *servo)
{
    if (!servo) {
        return 0;
    }
    return servo->current_angle;
}

int servo_motor_enable(servo_motor_t *servo, bool enable)
{
    if (!servo) {
        return -EINVAL;
    }

    if (enable && !servo->enabled) {
        servo->enabled = true;
        servo->pulse_end_time = k_uptime_ticks() + k_us_to_ticks_ceil32(servo->current_pulse_us);
        gpio_pin_set(servo->gpio_port, servo->gpio_pin, 1);
        LOG_INF("Servo enabled at %u degrees", servo->current_angle);
    } else if (!enable && servo->enabled) {
        gpio_pin_set(servo->gpio_port, servo->gpio_pin, 0);
        servo->enabled = false;
        LOG_INF("Servo disabled");
    }

    return 0;
}

bool servo_motor_is_enabled(const servo_motor_t *servo)
{
    if (!servo) {
        return false;
    }
    return servo->enabled;
}

void servo_motor_update(servo_motor_t *servo)
{
    if (!servo || !servo->enabled) {
        return;
    }

    uint64_t now = k_uptime_ticks();
    
    if (gpio_pin_get(servo->gpio_port, servo->gpio_pin) && now >= servo->pulse_end_time) {
        gpio_pin_set(servo->gpio_port, servo->gpio_pin, 0);
    }
}

