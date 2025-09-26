#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include "app/app.h"
#include "hal/gpio_matrix.h"
#include "hal/stepper.h"
#include "hal/pinmap.h"
#include "subsys/motion/planner.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// GPIO device references
#define GPIO_DEVICE_B DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define GPIO_DEVICE_C DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define GPIO_DEVICE_D DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define GPIO_DEVICE_E DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define GPIO_DEVICE_F DEVICE_DT_GET(DT_NODELABEL(gpiof))

// GPIO Matrix Configuration for 8x8 chess board scanning
static const matrix_cfg_t matrix_config = {
    .rows = {
        {.port = GPIO_DEVICE_B, .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_B, .pin = 1, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_B, .pin = 2, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_B, .pin = 3, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_B, .pin = 4, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_B, .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_B, .pin = 6, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_B, .pin = 7, .dt_flags = GPIO_ACTIVE_HIGH}
    },
    .cols = {
        {.port = GPIO_DEVICE_C, .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_C, .pin = 1, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_C, .pin = 2, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_C, .pin = 3, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_C, .pin = 4, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_C, .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_C, .pin = 6, .dt_flags = GPIO_ACTIVE_HIGH},
        {.port = GPIO_DEVICE_C, .pin = 7, .dt_flags = GPIO_ACTIVE_HIGH}
    },
    .active_high = true,
    .period_ms = 10
};

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

int main(void)
{
    LOG_INF("Booting Chess Robot Firmware v2.0");
    LOG_INF("Platform: STM32 Nucleo F767ZI with TB6600 stepper drivers");
    
    int rc = 0;
    
    // Initialize status LEDs first for visual feedback
    rc = init_status_leds();
    if (rc) {
        LOG_ERR("Failed to initialize status LEDs: %d", rc);
        return rc;
    }
    
    // Set red LED to indicate boot process
    set_status_led(0, true); // Red LED on
    
    // Initialize emergency stop
    rc = init_emergency_stop();
    if (rc) {
        LOG_ERR("Failed to initialize emergency stop: %d", rc);
        return rc;
    }
    
    // Initialize electromagnet control
    rc = init_electromagnet();
    if (rc) {
        LOG_ERR("Failed to initialize electromagnet: %d", rc);
        return rc;
    }
    
    // Initialize GPIO matrix scanner for chess board
    LOG_INF("Initializing chess board matrix scanner...");
    rc = gpio_matrix_init(&matrix_config);
    if (rc) {
        LOG_ERR("Failed to initialize GPIO matrix: %d", rc);
        return rc;
    }
    
    // Initialize motion planner with TB6600 stepper motors
    LOG_INF("Initializing TB6600 motion control system...");
    rc = planner_init(&motion_planner, &x_axis_config, &y_axis_config, &z_axis_config);
    if (rc) {
        LOG_ERR("Failed to initialize motion planner: %d", rc);
        return rc;
    }
    
    // Enable motion planner
    rc = planner_enable(&motion_planner, true);
    if (rc) {
        LOG_ERR("Failed to enable motion planner: %d", rc);
        return rc;
    }
    
    // Start matrix scanning
    LOG_INF("Starting chess board scanning...");
    gpio_matrix_start();
    
    // Initialize application layer
    LOG_INF("Starting application layer...");
    rc = app_start();
    if (rc) {
        LOG_ERR("Failed to start application: %d", rc);
        return rc;
    }
    
    // Boot sequence complete
    set_status_led(0, false); // Red LED off
    set_status_led(1, true);  // Green LED on
    
    LOG_INF("Chess robot system initialization complete");
    LOG_INF("Board scanning: Active");
    LOG_INF("Motion control: Ready");
    LOG_INF("MQTT communication: Starting...");
    
    // Main system monitoring loop
    while (true) {
        // Check emergency stop
        if (check_emergency_stop()) {
            LOG_WRN("EMERGENCY STOP ACTIVATED!");
            planner_emergency_stop(&motion_planner);
            set_status_led(0, true);  // Red LED on
            set_status_led(1, false); // Green LED off
            
            // Wait for emergency stop to be released
            while (check_emergency_stop()) {
                k_msleep(100);
            }
            
            LOG_INF("Emergency stop released - system requires restart");
            set_status_led(0, false);
            set_status_led(1, true);
        }
        
        // Update status indication based on system state
        if (planner_is_moving(&motion_planner)) {
            // Blue LED indicates motion
            set_status_led(2, true);
        } else {
            set_status_led(2, false);
        }
        
        // System heartbeat
        k_msleep(100);
    }
    
    return 0;
}