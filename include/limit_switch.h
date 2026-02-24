#ifndef LIMIT_SWITCH_H
#define LIMIT_SWITCH_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>

/* Forward declaration */
struct stepper_motor;
typedef struct stepper_motor stepper_motor_t;

/**
 * @brief Limit switch axis identifiers
 */
typedef enum {
    LIMIT_SWITCH_X = 0,
    LIMIT_SWITCH_Y,
    LIMIT_SWITCH_Z,
    LIMIT_SWITCH_MAX
} limit_switch_id_t;

/**
 * @brief Limit switch instance (opaque)
 */
typedef struct limit_switch limit_switch_t;

/**
 * @brief Callback invoked when limit switch is triggered
 * @note Called from ISR context - keep processing minimal!
 */
typedef void (*limit_switch_callback_t)(limit_switch_t *sw, void *user_data);

/**
 * @brief Initialize the limit switch subsystem
 * @return 0 on success, negative errno on failure
 */
int limit_switch_init(void);

/**
 * @brief Get a limit switch by ID
 * @param id Limit switch identifier
 * @return Pointer to limit switch, or NULL if not found
 */
limit_switch_t *limit_switch_get(limit_switch_id_t id);

/**
 * @brief Check if a limit switch is currently triggered
 * @param sw Pointer to limit switch
 * @return true if triggered (switch pressed), false otherwise
 */
bool limit_switch_is_triggered(const limit_switch_t *sw);

/**
 * @brief Attach a stepper motor to this limit switch
 * 
 * When the limit switch triggers, the attached motor(s) will be
 * immediately stopped via emergency stop (discarding step queue).
 * 
 * @param sw Pointer to limit switch
 * @param motor Pointer to stepper motor to attach
 * @return 0 on success, negative errno on failure
 */
int limit_switch_attach_motor(limit_switch_t *sw, stepper_motor_t *motor);

/**
 * @brief Attach a second motor to this limit switch (for Y-axis dual motors)
 * @param sw Pointer to limit switch
 * @param motor Pointer to second stepper motor to attach
 * @return 0 on success, negative errno on failure
 */
int limit_switch_attach_motor_secondary(limit_switch_t *sw, stepper_motor_t *motor);

/**
 * @brief Register a callback for when the limit switch triggers
 * @note Callback is invoked from ISR context!
 * @param sw Pointer to limit switch
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success, negative errno on failure
 */
int limit_switch_register_callback(limit_switch_t *sw, 
                                   limit_switch_callback_t callback,
                                   void *user_data);

/**
 * @brief Enable or disable the limit switch interrupt
 * @param sw Pointer to limit switch
 * @param enable true to enable, false to disable
 * @return 0 on success, negative errno on failure
 */
int limit_switch_enable_interrupt(limit_switch_t *sw, bool enable);

/**
 * @brief Check if a limit switch was triggered since last clear
 * @param sw Pointer to limit switch
 * @return true if triggered flag is set
 */
bool limit_switch_was_triggered(const limit_switch_t *sw);

/**
 * @brief Clear the triggered flag
 * @param sw Pointer to limit switch
 */
void limit_switch_clear_triggered(limit_switch_t *sw);

/**
 * @brief Poll all switches and enforce emergency stop if any is active.
 *
 * This is a safety redundancy path in case GPIO interrupts are missed or
 * unavailable. Call this periodically from the motor control task before
 * generating new step pulses.
 */
void limit_switch_poll_safety(void);

#endif /* LIMIT_SWITCH_H */
