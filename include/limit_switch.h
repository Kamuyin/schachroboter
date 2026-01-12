#ifndef LIMIT_SWITCH_H
#define LIMIT_SWITCH_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>

struct stepper_motor;
typedef struct stepper_motor stepper_motor_t;

typedef enum {
    LIMIT_SWITCH_X = 0,
    LIMIT_SWITCH_Y,
    LIMIT_SWITCH_Z,
    LIMIT_SWITCH_MAX
} limit_switch_id_t;

typedef struct limit_switch limit_switch_t;

typedef void (*limit_switch_callback_t)(limit_switch_t *sw, void *user_data);

int limit_switch_init(void);

limit_switch_t *limit_switch_get(limit_switch_id_t id);

bool limit_switch_is_triggered(const limit_switch_t *sw);

int limit_switch_attach_motor(limit_switch_t *sw, stepper_motor_t *motor);

int limit_switch_attach_motor_secondary(limit_switch_t *sw, stepper_motor_t *motor);

int limit_switch_register_callback(limit_switch_t *sw, 
                                   limit_switch_callback_t callback,
                                   void *user_data);

int limit_switch_enable_interrupt(limit_switch_t *sw, bool enable);

bool limit_switch_was_triggered(const limit_switch_t *sw);

void limit_switch_clear_triggered(limit_switch_t *sw);

#endif /* LIMIT_SWITCH_H */
