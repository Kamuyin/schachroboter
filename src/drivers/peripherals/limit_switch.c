#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "limit_switch.h"
#include "stepper_motor.h"

LOG_MODULE_REGISTER(limit_switch, LOG_LEVEL_INF);

/* Maximum motors that can be attached to a single limit switch */
#define MAX_ATTACHED_MOTORS 2

struct limit_switch {
    const struct device *out_port;
    gpio_pin_t out_pin;
    const struct device *in_port;
    gpio_pin_t in_pin;
    bool active_high;
    
    /* Attached motors for emergency stop */
    stepper_motor_t *motors[MAX_ATTACHED_MOTORS];
    uint8_t motor_count;
    
    /* Callback */
    limit_switch_callback_t callback;
    void *user_data;
    
    /* GPIO callback for interrupt */
    struct gpio_callback gpio_cb;
    
    /* State */
    volatile bool triggered_flag;
    volatile bool active_latched;
    uint32_t release_counter;
    uint32_t trigger_debounce_counter;
    bool interrupt_enabled;
    bool initialized;
};

/* Static storage for limit switches */
static struct limit_switch switches[LIMIT_SWITCH_MAX];

/* Forward declaration of ISR handler */
static void limit_switch_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins);

static inline bool read_switch_triggered(const struct limit_switch *sw)
{
    int val = gpio_pin_get(sw->in_port, sw->in_pin);
    if (val < 0) {
        return false;
    }
    return sw->active_high ? (val > 0) : (val == 0);
}

static void trigger_switch(struct limit_switch *sw)
{
    if (!sw || !sw->initialized) {
        return;
    }

    /* Already latched active: do not retrigger callbacks/logs repeatedly. */
    if (sw->active_latched) {
        return;
    }

    sw->active_latched = true;

    sw->triggered_flag = true;

    /* Unconditional emergency stop on all attached motors */
    for (int m = 0; m < sw->motor_count; m++) {
        if (sw->motors[m]) {
            stepper_motor_emergency_stop(sw->motors[m]);
        }
    }

    if (sw->callback) {
        sw->callback(sw, sw->user_data);
    }
}

/* Device tree node check macros */
#define LIMIT_SWITCH_X_NODE DT_NODELABEL(limit_switch_x)
#define LIMIT_SWITCH_Y_NODE DT_NODELABEL(limit_switch_y)
#define LIMIT_SWITCH_Z_NODE DT_NODELABEL(limit_switch_z)

#if DT_NODE_EXISTS(LIMIT_SWITCH_X_NODE)
#define HAVE_LIMIT_X 1
#else
#define HAVE_LIMIT_X 0
#endif

#if DT_NODE_EXISTS(LIMIT_SWITCH_Y_NODE)
#define HAVE_LIMIT_Y 1
#else
#define HAVE_LIMIT_Y 0
#endif

#if DT_NODE_EXISTS(LIMIT_SWITCH_Z_NODE)
#define HAVE_LIMIT_Z 1
#else
#define HAVE_LIMIT_Z 0
#endif

static int init_switch_from_dt(limit_switch_id_t id, 
                                const struct device *out_port, gpio_pin_t out_pin,
                                const struct device *in_port, gpio_pin_t in_pin,
                                bool active_high)
{
    struct limit_switch *sw = &switches[id];
    int ret;
    
    if (id >= LIMIT_SWITCH_MAX) {
        return -EINVAL;
    }
    
    memset(sw, 0, sizeof(*sw));
    
    sw->out_port = out_port;
    sw->out_pin = out_pin;
    sw->in_port = in_port;
    sw->in_pin = in_pin;
    sw->active_high = active_high;
    
    /* Check devices are ready */
    if (!device_is_ready(sw->out_port)) {
        LOG_ERR("Limit switch %d: OUT port not ready", id);
        return -ENODEV;
    }
    
    if (!device_is_ready(sw->in_port)) {
        LOG_ERR("Limit switch %d: IN port not ready", id);
        return -ENODEV;
    }
    
    /* Configure OUT pin as output, set HIGH to power the switch */
    ret = gpio_pin_configure(sw->out_port, sw->out_pin, GPIO_OUTPUT_HIGH);
    if (ret < 0) {
        LOG_ERR("Limit switch %d: Failed to configure OUT pin: %d", id, ret);
        return ret;
    }
    
    /* Configure IN pin as input with pull-down
     * For NO switch: unpressed = open = pulled LOW, pressed = connected to OUT = HIGH
     */
    ret = gpio_pin_configure(sw->in_port, sw->in_pin, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) {
        LOG_ERR("Limit switch %d: Failed to configure IN pin: %d", id, ret);
        return ret;
    }
    
    /* Configure interrupt on the appropriate edge */
    gpio_flags_t int_flags = active_high ? GPIO_INT_EDGE_TO_ACTIVE : GPIO_INT_EDGE_TO_INACTIVE;
    ret = gpio_pin_interrupt_configure(sw->in_port, sw->in_pin, int_flags);
    if (ret < 0) {
        LOG_ERR("Limit switch %d: Failed to configure interrupt: %d", id, ret);
        return ret;
    }
    
    /* Initialize GPIO callback */
    gpio_init_callback(&sw->gpio_cb, limit_switch_isr, BIT(sw->in_pin));
    ret = gpio_add_callback(sw->in_port, &sw->gpio_cb);
    if (ret < 0) {
        LOG_ERR("Limit switch %d: Failed to add GPIO callback: %d", id, ret);
        return ret;
    }
    
    sw->trigger_debounce_counter = 0;
    sw->interrupt_enabled = true;
    sw->initialized = true;

    int initial_level = gpio_pin_get(sw->in_port, sw->in_pin);
    bool initial_active = (initial_level >= 0) ? (sw->active_high ? (initial_level > 0) : (initial_level == 0)) : false;
    sw->active_latched = initial_active;

    LOG_INF("Limit switch %d initialized (active_high=%d, OUT=%s:%u, IN=%s:%u, IN_level=%d)",
            id,
            active_high,
            sw->out_port->name, sw->out_pin,
            sw->in_port->name, sw->in_pin,
            initial_level);

    if (initial_active) {
        LOG_WRN("Limit switch %d is already active at boot (line high)", id);
    }
    return 0;
}

static void limit_switch_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    /* Find which switch triggered */
    for (int i = 0; i < LIMIT_SWITCH_MAX; i++) {
        struct limit_switch *sw = &switches[i];
        
        if (!sw->initialized) {
            continue;
        }
        
        /* Check if this switch matches the triggered port/pin */
        if (sw->in_port == port && (pins & BIT(sw->in_pin))) {
            /* 
             * Simple glitch filter:
             * Verify that the switch is actually in the active state.
             * If this was just a noise spike that already passed, ignore it.
             */
            if (read_switch_triggered(sw)) {
                trigger_switch(sw);
            }
        }
    }
}

int limit_switch_init(void)
{
    int ret;
    int initialized_count = 0;
    
    LOG_INF("Initializing limit switches");
    
    memset(switches, 0, sizeof(switches));
    
#if HAVE_LIMIT_X
    ret = init_switch_from_dt(
        LIMIT_SWITCH_X,
        DEVICE_DT_GET(DT_GPIO_CTLR(LIMIT_SWITCH_X_NODE, out_gpios)),
        DT_GPIO_PIN(LIMIT_SWITCH_X_NODE, out_gpios),
        DEVICE_DT_GET(DT_GPIO_CTLR(LIMIT_SWITCH_X_NODE, in_gpios)),
        DT_GPIO_PIN(LIMIT_SWITCH_X_NODE, in_gpios),
        DT_NODE_HAS_PROP(LIMIT_SWITCH_X_NODE, active_high)
    );
    if (ret == 0) {
        initialized_count++;
    } else {
        LOG_WRN("Failed to initialize X limit switch: %d", ret);
    }
#endif

#if HAVE_LIMIT_Y
    ret = init_switch_from_dt(
        LIMIT_SWITCH_Y,
        DEVICE_DT_GET(DT_GPIO_CTLR(LIMIT_SWITCH_Y_NODE, out_gpios)),
        DT_GPIO_PIN(LIMIT_SWITCH_Y_NODE, out_gpios),
        DEVICE_DT_GET(DT_GPIO_CTLR(LIMIT_SWITCH_Y_NODE, in_gpios)),
        DT_GPIO_PIN(LIMIT_SWITCH_Y_NODE, in_gpios),
        DT_NODE_HAS_PROP(LIMIT_SWITCH_Y_NODE, active_high)
    );
    if (ret == 0) {
        initialized_count++;
    } else {
        LOG_WRN("Failed to initialize Y limit switch: %d", ret);
    }
#endif

#if HAVE_LIMIT_Z
    ret = init_switch_from_dt(
        LIMIT_SWITCH_Z,
        DEVICE_DT_GET(DT_GPIO_CTLR(LIMIT_SWITCH_Z_NODE, out_gpios)),
        DT_GPIO_PIN(LIMIT_SWITCH_Z_NODE, out_gpios),
        DEVICE_DT_GET(DT_GPIO_CTLR(LIMIT_SWITCH_Z_NODE, in_gpios)),
        DT_GPIO_PIN(LIMIT_SWITCH_Z_NODE, in_gpios),
        DT_NODE_HAS_PROP(LIMIT_SWITCH_Z_NODE, active_high)
    );
    if (ret == 0) {
        initialized_count++;
    } else {
        LOG_WRN("Failed to initialize Z limit switch: %d", ret);
    }
#endif

    LOG_INF("Limit switch subsystem initialized (%d switches)", initialized_count);
    return 0;
}

limit_switch_t *limit_switch_get(limit_switch_id_t id)
{
    if (id >= LIMIT_SWITCH_MAX) {
        return NULL;
    }
    
    if (!switches[id].initialized) {
        return NULL;
    }
    
    return &switches[id];
}

bool limit_switch_is_triggered(const limit_switch_t *sw)
{
    if (!sw || !sw->initialized) {
        return false;
    }

    return read_switch_triggered(sw);
}

int limit_switch_attach_motor(limit_switch_t *sw, stepper_motor_t *motor)
{
    if (!sw || !motor) {
        return -EINVAL;
    }
    
    if (sw->motor_count >= MAX_ATTACHED_MOTORS) {
        LOG_ERR("Cannot attach more motors to limit switch");
        return -ENOMEM;
    }
    
    sw->motors[sw->motor_count++] = motor;
    LOG_INF("Motor attached to limit switch (count=%d)", sw->motor_count);
    return 0;
}

int limit_switch_attach_motor_secondary(limit_switch_t *sw, stepper_motor_t *motor)
{
    /* Same as attach_motor, just clearer semantics for Y-axis dual motor */
    return limit_switch_attach_motor(sw, motor);
}

int limit_switch_register_callback(limit_switch_t *sw, 
                                   limit_switch_callback_t callback,
                                   void *user_data)
{
    if (!sw) {
        return -EINVAL;
    }
    
    sw->callback = callback;
    sw->user_data = user_data;
    return 0;
}

int limit_switch_enable_interrupt(limit_switch_t *sw, bool enable)
{
    if (!sw || !sw->initialized) {
        return -EINVAL;
    }
    
    gpio_flags_t flags;
    if (enable) {
        flags = sw->active_high ? GPIO_INT_EDGE_TO_ACTIVE : GPIO_INT_EDGE_TO_INACTIVE;
    } else {
        flags = GPIO_INT_DISABLE;
    }
    
    int ret = gpio_pin_interrupt_configure(sw->in_port, sw->in_pin, flags);
    if (ret == 0) {
        sw->interrupt_enabled = enable;
    }
    return ret;
}

bool limit_switch_was_triggered(const limit_switch_t *sw)
{
    if (!sw) {
        return false;
    }
    return sw->triggered_flag;
}

void limit_switch_debug_log_state(void)
{
    LOG_INF("--- Limit Switch Debug State ---");
    for (int i = 0; i < LIMIT_SWITCH_MAX; i++) {
        struct limit_switch *sw = &switches[i];
        if (!sw->initialized) {
            LOG_INF("Switch %d: Not Initialized", i);
            continue;
        }

        int val = gpio_pin_get(sw->in_port, sw->in_pin);
        LOG_INF("Switch %d (%s:%d): Raw=%d, Latched=%d, Debounce=%d", 
                i, sw->in_port->name, sw->in_pin, 
                val, sw->active_latched, sw->trigger_debounce_counter);
    }
    LOG_INF("--------------------------------");
}

#define RELEASE_DEBOUNCE_COUNT 100 
#define TRIGGER_DEBOUNCE_COUNT 3

void limit_switch_clear_triggered(limit_switch_t *sw)
{
    if (sw) {
        sw->triggered_flag = false;
        sw->active_latched = false;
    }
}

void limit_switch_safety_poll(void)
{
    for (int i = 0; i < LIMIT_SWITCH_MAX; i++) {
        struct limit_switch *sw = &switches[i];
        if (!sw->initialized) {
            continue;
        }

        bool active = read_switch_triggered(sw);
        if (active) {
            sw->trigger_debounce_counter++;
            if (sw->trigger_debounce_counter >= TRIGGER_DEBOUNCE_COUNT) {
              sw->release_counter = 0;
              trigger_switch(sw);
            }
        } else {
            sw->trigger_debounce_counter = 0;
            
            if (sw->active_latched) {
                /* Only unlatch if switch has been consistently inactive for some time */
                sw->release_counter++;
                if (sw->release_counter >= RELEASE_DEBOUNCE_COUNT) {
                    sw->active_latched = false;
                    sw->triggered_flag = false; /* Clear flag when physical switch releases */
                    sw->release_counter = 0;
                }
            }
        }
    }
}
