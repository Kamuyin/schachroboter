#pragma once
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

// TB6600 Stepper Driver Pin Definitions for STM32 Nucleo F767ZI
// X-Axis Stepper (TB6600 #1)
#define X_STEP_GPIO     DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define X_STEP_PIN      0
#define X_DIR_GPIO      DEVICE_DT_GET(DT_NODELABEL(gpiod))  
#define X_DIR_PIN       1
#define X_EN_GPIO       DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define X_EN_PIN        2

// Y-Axis Stepper (TB6600 #2)
#define Y_STEP_GPIO     DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define Y_STEP_PIN      3
#define Y_DIR_GPIO      DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define Y_DIR_PIN       4
#define Y_EN_GPIO       DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define Y_EN_PIN        5

// Z-Axis Stepper (TB6600 #3)
#define Z_STEP_GPIO     DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define Z_STEP_PIN      6
#define Z_DIR_GPIO      DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define Z_DIR_PIN       7
#define Z_EN_GPIO       DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define Z_EN_PIN        0

// Limit Switches (Homing)
#define X_LIMIT_GPIO    DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define X_LIMIT_PIN     1
#define Y_LIMIT_GPIO    DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define Y_LIMIT_PIN     2
#define Z_LIMIT_GPIO    DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define Z_LIMIT_PIN     3

// Emergency Stop
#define ESTOP_GPIO      DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define ESTOP_PIN       4

// Status LEDs
#define STATUS_LED_GPIO DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define STATUS_LED_PIN  14  // Built-in LED on Nucleo
#define ERROR_LED_GPIO  DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define ERROR_LED_PIN   5

// TB6600 Configuration Constants
#define TB6600_MIN_PULSE_WIDTH_US   2   // Minimum pulse width for TB6600
#define TB6600_MIN_SETUP_TIME_US    5   // Minimum setup time for direction
#define TB6600_ENABLE_ACTIVE_LOW    true // TB6600 enable is active low

// Stepper Motor Specifications (adjust based on your motors)
#define STEPS_PER_REV              200   // 1.8° stepper motor
#define MICROSTEP_MULTIPLIER       8     // TB6600 microstepping setting
#define TOTAL_STEPS_PER_REV        (STEPS_PER_REV * MICROSTEP_MULTIPLIER)

// Mechanical specifications (adjust based on your setup)
#define X_AXIS_MM_PER_REV          2.0f  // Belt pitch or lead screw pitch
#define Y_AXIS_MM_PER_REV          2.0f  
#define Z_AXIS_MM_PER_REV          2.0f

#define X_AXIS_STEPS_PER_MM        (TOTAL_STEPS_PER_REV / X_AXIS_MM_PER_REV)
#define Y_AXIS_STEPS_PER_MM        (TOTAL_STEPS_PER_REV / Y_AXIS_MM_PER_REV)
#define Z_AXIS_STEPS_PER_MM        (TOTAL_STEPS_PER_REV / Z_AXIS_MM_PER_REV)

// Motion limits
#define MAX_FEED_RATE_MM_S         50.0f   // Maximum feed rate
#define DEFAULT_ACCEL_MM_S2        100.0f  // Default acceleration
#define HOMING_FEED_RATE_MM_S      10.0f   // Homing speed
#define HOMING_BACKOFF_MM          2.0f    // Backoff distance after hitting limit

// Travel limits (adjust based on your machine)
#define X_MAX_TRAVEL_MM            200.0f
#define Y_MAX_TRAVEL_MM            200.0f
#define Z_MAX_TRAVEL_MM            100.0f

#ifdef __cplusplus
}
#endif