#include "hal/stepper.h"
#include "core/events.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>
LOG_MODULE_REGISTER(stepper, LOG_LEVEL_INF);

// Forward declarations
static void stepper_step_work_handler(struct k_work *work);
static int stepper_configure_gpio_pins(stepper_t *m);
static uint32_t stepper_calculate_step_period_us(float feed_rate_mm_s, int32_t steps_per_mm);
static int stepper_set_direction(stepper_t *m, bool positive);

static int stepper_configure_gpio_pins(stepper_t *m)
{
    int rc = 0;
    
    // Configure step pin as output
    if (!device_is_ready(m->cfg.step.port)) {
        LOG_ERR("%s: Step pin GPIO device not ready", m->cfg.axis_name);
        return -ENODEV;
    }
    rc |= gpio_pin_configure_dt(&m->cfg.step, GPIO_OUTPUT_INACTIVE);
    
    // Configure direction pin as output  
    if (!device_is_ready(m->cfg.dir.port)) {
        LOG_ERR("%s: Direction pin GPIO device not ready", m->cfg.axis_name);
        return -ENODEV;
    }
    rc |= gpio_pin_configure_dt(&m->cfg.dir, GPIO_OUTPUT_INACTIVE);
    
    // Configure enable pin as output
    if (!device_is_ready(m->cfg.en.port)) {
        LOG_ERR("%s: Enable pin GPIO device not ready", m->cfg.axis_name);
        return -ENODEV;
    }
    rc |= gpio_pin_configure_dt(&m->cfg.en, GPIO_OUTPUT_INACTIVE);
    
    // Configure limit switch as input with pull-up
    if (m->cfg.limit.port && device_is_ready(m->cfg.limit.port)) {
        rc |= gpio_pin_configure_dt(&m->cfg.limit, GPIO_INPUT | GPIO_PULL_UP);
    }
    
    return rc;
}

int stepper_init(stepper_t *m, const stepper_cfg_t *cfg)
{
    if (!m || !cfg) {
        return -EINVAL;
    }
    
    memset(m, 0, sizeof(stepper_t));
    m->cfg = *cfg;
    m->state = STEPPER_STATE_IDLE;
    m->enabled = false;
    m->homed = false;
    m->pos_mm = 0.0f;
    m->target_mm = 0.0f;
    m->remaining_steps = 0;
    m->current_step_period_us = 1000; // 1kHz default
    
    // Initialize mutex
    k_mutex_init(&m->mutex);
    
    // Initialize delayed work for step generation
    k_work_init_delayable(&m->step_work, stepper_step_work_handler);
    
    // Configure GPIO pins
    int rc = stepper_configure_gpio_pins(m);
    if (rc) {
        LOG_ERR("%s: Failed to configure GPIO pins: %d", m->cfg.axis_name, rc);
        return rc;
    }
    
    // Disable stepper initially
    rc = stepper_enable(m, false);
    if (rc) {
        LOG_ERR("%s: Failed to disable stepper: %d", m->cfg.axis_name, rc);
        return rc;
    }
    
    LOG_INF("%s: Stepper initialized successfully", m->cfg.axis_name);
    return 0;
}

int stepper_enable(stepper_t *m, bool on)
{
    if (!m) {
        return -EINVAL;
    }
    
    k_mutex_lock(&m->mutex, K_FOREVER);
    
    // TB6600 enable pin is typically active LOW
    int val = (m->cfg.inverted_en ? !on : on);
    int rc = gpio_pin_set_dt(&m->cfg.en, val);
    
    if (rc == 0) {
        m->enabled = on;
        LOG_DBG("%s: Stepper %s", m->cfg.axis_name, on ? "enabled" : "disabled");
        
        // Give TB6600 time to enable/disable
        k_msleep(TB6600_MIN_SETUP_TIME_US / 1000 + 1);
    } else {
        LOG_ERR("%s: Failed to %s stepper: %d", m->cfg.axis_name, on ? "enable" : "disable", rc);
    }
    
    k_mutex_unlock(&m->mutex);
    return rc;
}

bool stepper_is_limit_triggered(stepper_t *m)
{
    if (!m || !m->cfg.limit.port) {
        return false;
    }
    
    int val = gpio_pin_get_dt(&m->cfg.limit);
    return m->cfg.inverted_limit ? !val : val;
}

int stepper_home(stepper_t *m)
{
    if (!m) {
        return -EINVAL;
    }
    
    if (!m->enabled) {
        LOG_ERR("%s: Cannot home - stepper not enabled", m->cfg.axis_name);
        return -EACCES;
    }
    
    if (!m->cfg.limit.port) {
        LOG_ERR("%s: Cannot home - no limit switch configured", m->cfg.axis_name);
        return -ENOTSUP;
    }
    
    LOG_INF("%s: Starting homing sequence", m->cfg.axis_name);
    
    k_mutex_lock(&m->mutex, K_FOREVER);
    m->state = STEPPER_STATE_HOMING;
    
    // Move towards limit switch at homing speed
    int32_t homing_steps = (int32_t)(m->cfg.travel_limit_mm * m->cfg.steps_per_mm * 1.1f); // 110% of travel
    uint32_t step_period_us = stepper_calculate_step_period_us(HOMING_FEED_RATE_MM_S, m->cfg.steps_per_mm);
    
    // Set direction towards limit (negative direction typically)
    stepper_set_direction(m, false);
    
    k_mutex_unlock(&m->mutex);
    
    // Move until limit switch is triggered
    for (int32_t i = 0; i < homing_steps; i++) {
        if (stepper_is_limit_triggered(m)) {
            LOG_INF("%s: Limit switch triggered during homing", m->cfg.axis_name);
            break;
        }
        
        gpio_pin_set_dt(&m->cfg.step, 1);
        k_busy_wait(TB6600_MIN_PULSE_WIDTH_US);
        gpio_pin_set_dt(&m->cfg.step, 0);
        k_busy_wait(step_period_us - TB6600_MIN_PULSE_WIDTH_US);
    }
    
    // Back off from limit switch
    stepper_set_direction(m, true);
    int32_t backoff_steps = (int32_t)(HOMING_BACKOFF_MM * m->cfg.steps_per_mm);
    
    for (int32_t i = 0; i < backoff_steps; i++) {
        gpio_pin_set_dt(&m->cfg.step, 1);
        k_busy_wait(TB6600_MIN_PULSE_WIDTH_US);
        gpio_pin_set_dt(&m->cfg.step, 0);
        k_busy_wait(step_period_us - TB6600_MIN_PULSE_WIDTH_US);
    }
    
    k_mutex_lock(&m->mutex, K_FOREVER);
    m->pos_mm = 0.0f; // Set current position as home
    m->homed = true;
    m->state = STEPPER_STATE_IDLE;
    k_mutex_unlock(&m->mutex);
    
    LOG_INF("%s: Homing completed successfully", m->cfg.axis_name);
    
    // Post homing done event
    event_t ev = {.type = EV_MOTION_DONE, .ts = k_uptime_get()};
    events_post(&ev, K_NO_WAIT);
    
    return 0;
}

static uint32_t stepper_calculate_step_period_us(float feed_rate_mm_s, int32_t steps_per_mm)
{
    if (feed_rate_mm_s <= 0) {
        return 1000; // 1kHz default
    }
    
    float steps_per_sec = feed_rate_mm_s * steps_per_mm;
    uint32_t period_us = (uint32_t)(1000000.0f / steps_per_sec);
    
    // Ensure minimum period for TB6600
    if (period_us < TB6600_MIN_PULSE_WIDTH_US * 2) {
        period_us = TB6600_MIN_PULSE_WIDTH_US * 2;
    }
    
    return period_us;
}

static int stepper_set_direction(stepper_t *m, bool positive)
{
    if (!m) {
        return -EINVAL;
    }
    
    gpio_pin_set_dt(&m->cfg.dir, positive ? 1 : 0);
    m->direction_positive = positive;
    
    // TB6600 needs setup time after direction change
    k_busy_wait(TB6600_MIN_SETUP_TIME_US);
    
    return 0;
}

static void stepper_step_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    stepper_t *m = CONTAINER_OF(dwork, stepper_t, step_work);
    
    if (!m->enabled || m->remaining_steps == 0) {
        k_mutex_lock(&m->mutex, K_FOREVER);
        m->state = STEPPER_STATE_IDLE;
        k_mutex_unlock(&m->mutex);
        
        // Post motion done event
        event_t ev = {.type = EV_MOTION_DONE, .ts = k_uptime_get()};
        events_post(&ev, K_NO_WAIT);
        return;
    }
    
    // Check limits during movement
    if (stepper_is_limit_triggered(m)) {
        LOG_WRN("%s: Limit switch triggered during movement", m->cfg.axis_name);
        k_mutex_lock(&m->mutex, K_FOREVER);
        m->remaining_steps = 0;
        m->state = STEPPER_STATE_ERROR;
        k_mutex_unlock(&m->mutex);
        return;
    }
    
    // Generate step pulse
    gpio_pin_set_dt(&m->cfg.step, 1);
    k_busy_wait(TB6600_MIN_PULSE_WIDTH_US);
    gpio_pin_set_dt(&m->cfg.step, 0);
    
    // Update position and remaining steps
    k_mutex_lock(&m->mutex, K_FOREVER);
    m->remaining_steps--;
    
    float step_mm = 1.0f / m->cfg.steps_per_mm;
    if (m->direction_positive) {
        m->pos_mm += step_mm;
    } else {
        m->pos_mm -= step_mm;
    }
    
    // Schedule next step if more steps remain
    if (m->remaining_steps > 0) {
        k_work_reschedule(&m->step_work, K_USEC(m->current_step_period_us - TB6600_MIN_PULSE_WIDTH_US));
    }
    k_mutex_unlock(&m->mutex);
}

int stepper_move_steps_async(stepper_t *m, int32_t steps, float feed_rate_mm_s)
{
    if (!m) {
        return -EINVAL;
    }
    
    if (!m->enabled) {
        LOG_ERR("%s: Cannot move - stepper not enabled", m->cfg.axis_name);
        return -EACCES;
    }
    
    if (steps == 0) {
        return 0;
    }
    
    k_mutex_lock(&m->mutex, K_FOREVER);
    
    if (m->state != STEPPER_STATE_IDLE) {
        k_mutex_unlock(&m->mutex);
        LOG_ERR("%s: Cannot move - stepper busy", m->cfg.axis_name);
        return -EBUSY;
    }
    
    // Set direction
    bool positive_dir = steps > 0;
    stepper_set_direction(m, positive_dir);
    
    // Setup movement parameters
    m->remaining_steps = abs(steps);
    m->current_step_period_us = stepper_calculate_step_period_us(feed_rate_mm_s, m->cfg.steps_per_mm);
    m->state = STEPPER_STATE_MOVING;
    
    k_mutex_unlock(&m->mutex);
    
    // Start step generation
    k_work_schedule(&m->step_work, K_NO_WAIT);
    
    LOG_DBG("%s: Started async movement: %d steps at %.1f mm/s", 
            m->cfg.axis_name, steps, feed_rate_mm_s);
    
    return 0;
}

int stepper_move_mm_async(stepper_t *m, float delta_mm, float feed_rate_mm_s)
{
    if (!m) {
        return -EINVAL;
    }
    
    int32_t steps = (int32_t)(delta_mm * m->cfg.steps_per_mm);
    return stepper_move_steps_async(m, steps, feed_rate_mm_s);
}

int stepper_move_to_async(stepper_t *m, float target_mm, float feed_rate_mm_s)
{
    if (!m) {
        return -EINVAL;
    }
    
    float delta_mm = target_mm - m->pos_mm;
    m->target_mm = target_mm;
    
    return stepper_move_mm_async(m, delta_mm, feed_rate_mm_s);
}

int stepper_wait_for_completion(stepper_t *m, k_timeout_t timeout)
{
    if (!m) {
        return -EINVAL;
    }
    
    int64_t start_time = k_uptime_get();
    int64_t timeout_ms = timeout.ticks == K_FOREVER.ticks ? INT64_MAX : k_ticks_to_ms_floor64(timeout.ticks);
    
    while (m->state == STEPPER_STATE_MOVING || m->state == STEPPER_STATE_HOMING) {
        if (k_uptime_get() - start_time > timeout_ms) {
            return -ETIMEDOUT;
        }
        k_msleep(1);
    }
    
    return 0;
}

int stepper_stop(stepper_t *m)
{
    if (!m) {
        return -EINVAL;
    }
    
    k_mutex_lock(&m->mutex, K_FOREVER);
    m->remaining_steps = 0;
    m->state = STEPPER_STATE_IDLE;
    k_mutex_unlock(&m->mutex);
    
    k_work_cancel_delayable(&m->step_work);
    
    LOG_DBG("%s: Movement stopped", m->cfg.axis_name);
    return 0;
}

int stepper_emergency_stop(stepper_t *m)
{
    if (!m) {
        return -EINVAL;
    }
    
    // Immediate stop
    stepper_stop(m);
    stepper_enable(m, false);
    
    k_mutex_lock(&m->mutex, K_FOREVER);
    m->state = STEPPER_STATE_ERROR;
    k_mutex_unlock(&m->mutex);
    
    LOG_WRN("%s: Emergency stop activated", m->cfg.axis_name);
    return 0;
}

// Synchronous versions for compatibility
int stepper_move_steps(stepper_t *m, int32_t steps, uint32_t step_us)
{
    float feed_rate = 1000000.0f / (step_us * m->cfg.steps_per_mm);
    int rc = stepper_move_steps_async(m, steps, feed_rate);
    if (rc == 0) {
        rc = stepper_wait_for_completion(m, K_FOREVER);
    }
    return rc;
}

int stepper_move_mm(stepper_t *m, float delta_mm)
{
    int rc = stepper_move_mm_async(m, delta_mm, m->cfg.max_feed_mm_s * 0.5f); // 50% of max speed
    if (rc == 0) {
        rc = stepper_wait_for_completion(m, K_FOREVER);
    }
    return rc;
}

// Status functions
stepper_state_t stepper_get_state(stepper_t *m)
{
    return m ? m->state : STEPPER_STATE_ERROR;
}

float stepper_get_position(stepper_t *m)
{
    return m ? m->pos_mm : 0.0f;
}

bool stepper_is_homed(stepper_t *m)
{
    return m ? m->homed : false;
}

bool stepper_is_moving(stepper_t *m)
{
    return m ? (m->state == STEPPER_STATE_MOVING || m->state == STEPPER_STATE_HOMING) : false;
}