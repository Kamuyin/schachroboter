#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include "app/app.h"
#include "hal/gpio_matrix.h"
// Temporarily disabled components for chess board testing phase
// #include "hal/stepper.h"
// #include "hal/pinmap.h"
// #include "subsys/motion/planner.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// GPIO device references
#define GPIO_DEVICE_A DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define GPIO_DEVICE_B DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define GPIO_DEVICE_C DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define GPIO_DEVICE_D DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define GPIO_DEVICE_E DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define GPIO_DEVICE_F DEVICE_DT_GET(DT_NODELABEL(gpiof))

// GPIO Matrix Configuration for 8x8 chess board scanning
static const matrix_cfg_t matrix_config = {
    .rows = {
        {.port = GPIO_DEVICE_D, .pin = 13, .dt_flags = 0},
        {.port = GPIO_DEVICE_D, .pin = 12, .dt_flags = 0},
        {.port = GPIO_DEVICE_D, .pin = 11, .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 10, .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 12, .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 14, .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 15, .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 13, .dt_flags = 0}
    },
    .cols = {
        {.port = GPIO_DEVICE_F, .pin = 4,  .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 8,  .dt_flags = 0},
        {.port = GPIO_DEVICE_F, .pin = 10, .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 7,  .dt_flags = 0},
        {.port = GPIO_DEVICE_D, .pin = 14, .dt_flags = 0},
        {.port = GPIO_DEVICE_D, .pin = 15, .dt_flags = 0},
        {.port = GPIO_DEVICE_F, .pin = 14, .dt_flags = 0},
        {.port = GPIO_DEVICE_E, .pin = 9,  .dt_flags = 0}
    },

    .active_high = true,

    .period_ms = 10
};

// =============================================================================
// TEMPORARILY DISABLED COMPONENTS FOR CHESS BOARD TESTING PHASE
// =============================================================================
/*
// TB6600 Stepper Motor Configurations
static const stepper_cfg_t x_axis_config = {
    .step = {.port = X_STEP_GPIO, .pin = X_STEP_PIN, .dt_flags = GPIO_ACTIVE_HIGH},
    .dir = {.port = X_DIR_GPIO, .pin = X_DIR_PIN, .dt_flags = GPIO_ACTIVE_HIGH},
    .en = {.port = X_EN_GPIO, .pin = X_EN_PIN, .dt_flags = GPIO_ACTIVE_LOW},
    .limit = {.port = X_LIMIT_GPIO, .pin = X_LIMIT_PIN, .dt_flags = GPIO_ACTIVE_LOW},
    .steps_per_mm = 80,  // X_STEPS_PER_MM from pinmap.h
    .max_feed_mm_s = 50.0f,  // X_MAX_FEED_MM_S from pinmap.h
    .travel_limit_mm = 250.0f,  // X_MAX_TRAVEL_MM from pinmap.h
    .inverted_en = true,
    .inverted_limit = true,
    .axis_name = "X"
};

static const stepper_cfg_t y_axis_config = {
    .step = {.port = Y_STEP_GPIO, .pin = Y_STEP_PIN, .dt_flags = GPIO_ACTIVE_HIGH},
    .dir = {.port = Y_DIR_GPIO, .pin = Y_DIR_PIN, .dt_flags = GPIO_ACTIVE_HIGH},
    .en = {.port = Y_EN_GPIO, .pin = Y_EN_PIN, .dt_flags = GPIO_ACTIVE_LOW},
    .limit = {.port = Y_LIMIT_GPIO, .pin = Y_LIMIT_PIN, .dt_flags = GPIO_ACTIVE_LOW},
    .steps_per_mm = 80,  // Y_STEPS_PER_MM from pinmap.h
    .max_feed_mm_s = 50.0f,  // Y_MAX_FEED_MM_S from pinmap.h
    .travel_limit_mm = 250.0f,  // Y_MAX_TRAVEL_MM from pinmap.h
    .inverted_en = true,
    .inverted_limit = true,
    .axis_name = "Y"
};

static const stepper_cfg_t z_axis_config = {
    .step = {.port = Z_STEP_GPIO, .pin = Z_STEP_PIN, .dt_flags = GPIO_ACTIVE_HIGH},
    .dir = {.port = Z_DIR_GPIO, .pin = Z_DIR_PIN, .dt_flags = GPIO_ACTIVE_HIGH},
    .en = {.port = Z_EN_GPIO, .pin = Z_EN_PIN, .dt_flags = GPIO_ACTIVE_LOW},
    .limit = {.port = Z_LIMIT_GPIO, .pin = Z_LIMIT_PIN, .dt_flags = GPIO_ACTIVE_LOW},
    .steps_per_mm = 400,  // Z_STEPS_PER_MM from pinmap.h
    .max_feed_mm_s = 25.0f,  // Z_MAX_FEED_MM_S from pinmap.h
    .travel_limit_mm = 50.0f,  // Z_MAX_TRAVEL_MM from pinmap.h
    .inverted_en = true,
    .inverted_limit = true,
    .axis_name = "Z"
};

// Global motion planner instance
static planner_t motion_planner;

// Emergency stop GPIO
static const struct gpio_dt_spec emergency_stop_gpio = {
    .port = ESTOP_GPIO,
    .pin = ESTOP_PIN,
    .dt_flags = GPIO_ACTIVE_LOW
};

// Status LEDs - using built-in LED and additional GPIO pins
static const struct gpio_dt_spec status_leds[] = {
    {.port = ERROR_LED_GPIO, .pin = ERROR_LED_PIN, .dt_flags = GPIO_ACTIVE_HIGH},  // Red
    {.port = STATUS_LED_GPIO, .pin = STATUS_LED_PIN, .dt_flags = GPIO_ACTIVE_HIGH}, // Green (built-in)
    {.port = GPIO_DEVICE_F, .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH}  // Blue (additional)
};

// Electromagnet control - using a spare GPIO pin
static const struct gpio_dt_spec electromagnet_gpio = {
    .port = GPIO_DEVICE_F,
    .pin = 1,
    .dt_flags = GPIO_ACTIVE_HIGH
};
*/

// =============================================================================
// ACTIVE COMPONENTS FOR CHESS BOARD TESTING 
// =============================================================================
// Built-in LED for basic status indication
static const struct gpio_dt_spec builtin_led = {
    .port = GPIO_DEVICE_B,
    .pin = 14,  // Built-in LED on Nucleo F767ZI 
    .dt_flags = GPIO_ACTIVE_HIGH
};

// =============================================================================
// DISABLED FUNCTIONS FOR TESTING PHASE
// =============================================================================
/*
static int init_status_leds(void)
{
    int rc = 0;
    
    for (int i = 0; i < ARRAY_SIZE(status_leds); i++) {
        if (!device_is_ready(status_leds[i].port)) {
            LOG_ERR("Status LED %d GPIO device not ready", i);
            return -ENODEV;
        }
        rc |= gpio_pin_configure_dt(&status_leds[i], GPIO_OUTPUT_INACTIVE);
    }
    
    if (rc == 0) {
        LOG_INF("Status LEDs initialized successfully");
        // Flash green LED to indicate successful initialization
        gpio_pin_set_dt(&status_leds[1], 1);
        k_msleep(200);
        gpio_pin_set_dt(&status_leds[1], 0);
    }
    
    return rc;
}

static int init_emergency_stop(void)
{
    if (!device_is_ready(emergency_stop_gpio.port)) {
        LOG_ERR("Emergency stop GPIO device not ready");
        return -ENODEV;
    }
    
    int rc = gpio_pin_configure_dt(&emergency_stop_gpio, GPIO_INPUT | GPIO_PULL_UP);
    if (rc == 0) {
        LOG_INF("Emergency stop initialized successfully");
    }
    
    return rc;
}

static int init_electromagnet(void)
{
    if (!device_is_ready(electromagnet_gpio.port)) {
        LOG_ERR("Electromagnet GPIO device not ready");
        return -ENODEV;
    }
    
    int rc = gpio_pin_configure_dt(&electromagnet_gpio, GPIO_OUTPUT_INACTIVE);
    if (rc == 0) {
        LOG_INF("Electromagnet control initialized successfully");
    }
    
    return rc;
}

static bool check_emergency_stop(void)
{
    return gpio_pin_get_dt(&emergency_stop_gpio) == 0; // Active LOW
}

static void set_status_led(int led_index, bool state)
{
    if (led_index >= 0 && led_index < ARRAY_SIZE(status_leds)) {
        gpio_pin_set_dt(&status_leds[led_index], state ? 1 : 0);
    }
}
*/

// =============================================================================
// SIMPLIFIED FUNCTIONS FOR CHESS BOARD TESTING
// =============================================================================
static int init_builtin_led(void)
{
    if (!device_is_ready(builtin_led.port)) {
        LOG_ERR("Built-in LED GPIO device not ready");
        return -ENODEV;
    }
    
    int rc = gpio_pin_configure_dt(&builtin_led, GPIO_OUTPUT_INACTIVE);
    if (rc == 0) {
        LOG_INF("Built-in LED initialized successfully");
        // Flash LED to indicate successful initialization
        gpio_pin_set_dt(&builtin_led, 1);
        k_msleep(200);
        gpio_pin_set_dt(&builtin_led, 0);
    }
    
    return rc;
}

static void set_builtin_led(bool state)
{
    gpio_pin_set_dt(&builtin_led, state ? 1 : 0);
}

int main(void)
{
    LOG_INF("Booting Chess Robot Firmware - TESTING MODE");
    LOG_INF("Platform: STM32 Nucleo F767ZI");
    LOG_INF("Active Components: Chess Board Detection Only");
    
    int rc = 0;
    
    // Initialize built-in LED for basic status indication
    rc = init_builtin_led();
    if (rc) {
        LOG_ERR("Failed to initialize built-in LED: %d", rc);
        return rc;
    }
    
    // Set LED on to indicate boot process
    set_builtin_led(true);
    
    // Initialize GPIO matrix scanner for chess board - THIS IS THE MAIN COMPONENT
    LOG_INF("Initializing chess board matrix scanner...");
    rc = gpio_matrix_init(&matrix_config);
    if (rc) {
        LOG_ERR("Failed to initialize GPIO matrix: %d", rc);
        set_builtin_led(false);  // Turn off LED to indicate failure
        return rc;
    }
    
    // Start matrix scanning
    LOG_INF("Starting chess board scanning...");
    gpio_matrix_start();
    
    // Initialize application layer (only for board state management)
    LOG_INF("Starting application layer (board detection only)...");
    rc = app_start();
    if (rc) {
        LOG_ERR("Failed to start application: %d", rc);
        set_builtin_led(false);  // Turn off LED to indicate failure
        return rc;
    }
    
    // Boot sequence complete - flash LED to indicate success
    for (int i = 0; i < 3; i++) {
        set_builtin_led(false);
        k_msleep(100);
        set_builtin_led(true);
        k_msleep(100);
    }
    
    LOG_INF("Chess board testing system initialization complete");
    LOG_INF("Board scanning: Active");
    LOG_INF("Motion control: DISABLED");
    LOG_INF("MQTT communication: DISABLED");
    LOG_INF("Emergency stop: DISABLED");
    LOG_INF("Stepper motors: DISABLED");
    
    // Simple main loop for board testing
    bool led_state = false;
    uint32_t heartbeat_counter = 0;
    
    while (true) {
        // Heartbeat LED (slow blink every 2 seconds to show system is alive)
        heartbeat_counter++;
        if (heartbeat_counter >= 20) {  // 20 * 100ms = 2 seconds
            led_state = !led_state;
            set_builtin_led(led_state);
            heartbeat_counter = 0;
            
            // Log system status periodically
            LOG_INF("System alive - Board scanning active");
        }
        
        // Simple system heartbeat
        k_msleep(100);
    }
    
    return 0;
}