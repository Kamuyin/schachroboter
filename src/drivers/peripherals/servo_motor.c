#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "servo_motor.h"

LOG_MODULE_REGISTER(servo_motor, LOG_LEVEL_INF);

struct servo_motor {
    const struct device *gpio_port;
    uint32_t gpio_pin;
    gpio_dt_flags_t gpio_flags;
    uint16_t current_angle;
    uint32_t current_pulse_us;
    volatile bool enabled;
};

/* Single global servo instance – registered by servo_motor_create() */
static servo_motor_t *g_servo = NULL;

/* ─────────────────────────────────────────────────────────────────────────────
 * Dedicated PWM thread
 *
 * HIGH phase: k_busy_wait(pulse_us)  – busy-spin for microsecond accuracy
 * LOW  phase: k_msleep(18)           – ~18 ms sleep; CPU is free for steppers
 *
 * Total cycle ≈ 18.5–20.5 ms (~49–54 Hz).
 * RC servos accept 15–25 ms periods; only pulse width determines position.
 *
 * Thread runs at priority 4 (higher than the rest at 5) so it cannot be
 * preempted during the short busy-wait, guaranteeing pulse-width accuracy.
 * ─────────────────────────────────────────────────────────────────────────────*/
static void servo_pwm_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        servo_motor_t *s = g_servo;

        if (!s) {
            // Wait for servo to be created
             k_msleep(100);
             continue;
        }

        if (!s->enabled) {
            // Be explicit about inactive state while disabled
            gpio_pin_set(s->gpio_port, s->gpio_pin, 0); 
            k_msleep(20);
            continue;
        }

        uint32_t pulse_us = s->current_pulse_us;

        /* Accurate HIGH pulse */
        gpio_pin_set(s->gpio_port, s->gpio_pin, 1);
        k_busy_wait(pulse_us);
        gpio_pin_set(s->gpio_port, s->gpio_pin, 0);

        /* LOW gap – sleep frees the CPU for stepper and network threads */
        k_msleep(18);
    }
}

K_THREAD_DEFINE(servo_pwm_tid, 2048,
                servo_pwm_thread_fn, NULL, NULL, NULL,
                4, 0, 0);

servo_motor_t *servo_motor_create(const struct device *gpio_port, uint32_t gpio_pin, gpio_dt_flags_t gpio_flags)
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
    servo->gpio_flags = gpio_flags;
    servo->current_angle = 90;
    servo->current_pulse_us = (SERVO_MIN_PULSE_US + SERVO_MAX_PULSE_US) / 2;
    servo->enabled = false;

    /* Register as the single active servo for the PWM thread */
    g_servo = servo;

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

    int ret = gpio_pin_configure(servo->gpio_port, servo->gpio_pin, GPIO_OUTPUT_INACTIVE | servo->gpio_flags);
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
        LOG_INF("Servo enabled at %u degrees", servo->current_angle);
    } else if (!enable && servo->enabled) {
        servo->enabled = false;
        gpio_pin_set(servo->gpio_port, servo->gpio_pin, 0);
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
    /* PWM is now driven by the dedicated servo_pwm_thread.
     * This function is intentionally a no-op kept for API compatibility. */
    ARG_UNUSED(servo);
}
