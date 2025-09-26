#pragma once
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <stdbool.h>
#include "hal/pinmap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STEPPER_STATE_IDLE,
    STEPPER_STATE_MOVING,
    STEPPER_STATE_HOMING,
    STEPPER_STATE_ERROR
} stepper_state_t;

typedef struct {
    struct gpio_dt_spec step, dir, en, limit;
    bool inverted_en;
    bool inverted_limit;
    int32_t steps_per_mm;
    float max_feed_mm_s;
    float accel_mm_s2;
    float travel_limit_mm;
    const char *axis_name;
} stepper_cfg_t;

typedef struct {
    stepper_cfg_t cfg;
    stepper_state_t state;
    bool enabled;
    bool homed;
    float pos_mm;
    float target_mm;
    int64_t last_step_time;
    uint32_t current_step_period_us;
    struct k_work_delayable step_work;
    struct k_mutex mutex;
    int32_t remaining_steps;
    bool direction_positive;
    k_tid_t motion_thread_id;
} stepper_t;

// TB6600-specific functions
int stepper_init(stepper_t *m, const stepper_cfg_t *cfg);
int stepper_enable(stepper_t *m, bool on);
int stepper_home(stepper_t *m);
bool stepper_is_limit_triggered(stepper_t *m);
int stepper_move_steps_async(stepper_t *m, int32_t steps, float feed_rate_mm_s);
int stepper_move_mm_async(stepper_t *m, float delta_mm, float feed_rate_mm_s);
int stepper_move_to_async(stepper_t *m, float target_mm, float feed_rate_mm_s);
int stepper_wait_for_completion(stepper_t *m, k_timeout_t timeout);
int stepper_stop(stepper_t *m);
int stepper_emergency_stop(stepper_t *m);

// Synchronous versions (for compatibility)
int stepper_move_steps(stepper_t *m, int32_t steps, uint32_t step_us);
int stepper_move_mm(stepper_t *m, float delta_mm);

// Status functions
stepper_state_t stepper_get_state(stepper_t *m);
float stepper_get_position(stepper_t *m);
bool stepper_is_homed(stepper_t *m);
bool stepper_is_moving(stepper_t *m);

#ifdef __cplusplus
}
#endif