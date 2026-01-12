#ifndef STEPPER_CONFIG_H
#define STEPPER_CONFIG_H

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/dt-bindings/pwm/pwm.h>
#include <zephyr/sys/util.h>

BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_x), okay), "stepper_x DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_y1), okay), "stepper_y1 DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_y2), okay), "stepper_y2 DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_z), okay), "stepper_z DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_gripper), okay), "stepper_gripper DT node required");

// X-Axis
#define STEPPER_X_PWM_DEV     DEVICE_DT_GET(DT_PWMS_CTLR(DT_NODELABEL(stepper_x)))
#define STEPPER_X_PWM_CHANNEL DT_PWMS_CHANNEL(DT_NODELABEL(stepper_x))
#define STEPPER_X_PWM_FLAGS   DT_PWMS_FLAGS(DT_NODELABEL(stepper_x))
#define STEPPER_X_DIR_PORT    DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_x), dir_gpios))
#define STEPPER_X_DIR_PIN     DT_GPIO_PIN(DT_NODELABEL(stepper_x), dir_gpios)
#define STEPPER_X_ENABLE_PORT DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_x), enable_gpios))
#define STEPPER_X_ENABLE_PIN  DT_GPIO_PIN(DT_NODELABEL(stepper_x), enable_gpios)
#define STEPPER_X_HAS_ENABLE  DT_NODE_HAS_PROP(DT_NODELABEL(stepper_x), enable_gpios)
#define STEPPER_X_DIR_INV     DT_NODE_HAS_PROP(DT_NODELABEL(stepper_x), dir_inverted)
#define STEPPER_X_MAX_SPEED   DT_PROP_OR(DT_NODELABEL(stepper_x), max_speed_hz, 5000)

// Y1-Axis
#define STEPPER_Y1_PWM_DEV     DEVICE_DT_GET(DT_PWMS_CTLR(DT_NODELABEL(stepper_y1)))
#define STEPPER_Y1_PWM_CHANNEL DT_PWMS_CHANNEL(DT_NODELABEL(stepper_y1))
#define STEPPER_Y1_PWM_FLAGS   DT_PWMS_FLAGS(DT_NODELABEL(stepper_y1))
#define STEPPER_Y1_DIR_PORT    DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_y1), dir_gpios))
#define STEPPER_Y1_DIR_PIN     DT_GPIO_PIN(DT_NODELABEL(stepper_y1), dir_gpios)
#define STEPPER_Y1_ENABLE_PORT DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_y1), enable_gpios))
#define STEPPER_Y1_ENABLE_PIN  DT_GPIO_PIN(DT_NODELABEL(stepper_y1), enable_gpios)
#define STEPPER_Y1_HAS_ENABLE  DT_NODE_HAS_PROP(DT_NODELABEL(stepper_y1), enable_gpios)
#define STEPPER_Y1_DIR_INV     DT_NODE_HAS_PROP(DT_NODELABEL(stepper_y1), dir_inverted)
#define STEPPER_Y1_MAX_SPEED   DT_PROP_OR(DT_NODELABEL(stepper_y1), max_speed_hz, 5000)

// Y2-Axis
#define STEPPER_Y2_PWM_DEV     DEVICE_DT_GET(DT_PWMS_CTLR(DT_NODELABEL(stepper_y2)))
#define STEPPER_Y2_PWM_CHANNEL DT_PWMS_CHANNEL(DT_NODELABEL(stepper_y2))
#define STEPPER_Y2_PWM_FLAGS   DT_PWMS_FLAGS(DT_NODELABEL(stepper_y2))
#define STEPPER_Y2_DIR_PORT    DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_y2), dir_gpios))
#define STEPPER_Y2_DIR_PIN     DT_GPIO_PIN(DT_NODELABEL(stepper_y2), dir_gpios)
#define STEPPER_Y2_ENABLE_PORT DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_y2), enable_gpios))
#define STEPPER_Y2_ENABLE_PIN  DT_GPIO_PIN(DT_NODELABEL(stepper_y2), enable_gpios)
#define STEPPER_Y2_HAS_ENABLE  DT_NODE_HAS_PROP(DT_NODELABEL(stepper_y2), enable_gpios)
#define STEPPER_Y2_DIR_INV     DT_NODE_HAS_PROP(DT_NODELABEL(stepper_y2), dir_inverted)
#define STEPPER_Y2_MAX_SPEED   DT_PROP_OR(DT_NODELABEL(stepper_y2), max_speed_hz, 5000)

// Z-Axis
#define STEPPER_Z_PWM_DEV     DEVICE_DT_GET(DT_PWMS_CTLR(DT_NODELABEL(stepper_z)))
#define STEPPER_Z_PWM_CHANNEL DT_PWMS_CHANNEL(DT_NODELABEL(stepper_z))
#define STEPPER_Z_PWM_FLAGS   DT_PWMS_FLAGS(DT_NODELABEL(stepper_z))
#define STEPPER_Z_DIR_PORT    DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_z), dir_gpios))
#define STEPPER_Z_DIR_PIN     DT_GPIO_PIN(DT_NODELABEL(stepper_z), dir_gpios)
#define STEPPER_Z_ENABLE_PORT DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_z), enable_gpios))
#define STEPPER_Z_ENABLE_PIN  DT_GPIO_PIN(DT_NODELABEL(stepper_z), enable_gpios)
#define STEPPER_Z_HAS_ENABLE  DT_NODE_HAS_PROP(DT_NODELABEL(stepper_z), enable_gpios)
#define STEPPER_Z_DIR_INV     DT_NODE_HAS_PROP(DT_NODELABEL(stepper_z), dir_inverted)
#define STEPPER_Z_MAX_SPEED   DT_PROP_OR(DT_NODELABEL(stepper_z), max_speed_hz, 5000)

// Gripper
#define STEPPER_GRIPPER_PWM_DEV     DEVICE_DT_GET(DT_PWMS_CTLR(DT_NODELABEL(stepper_gripper)))
#define STEPPER_GRIPPER_PWM_CHANNEL DT_PWMS_CHANNEL(DT_NODELABEL(stepper_gripper))
#define STEPPER_GRIPPER_PWM_FLAGS   DT_PWMS_FLAGS(DT_NODELABEL(stepper_gripper))
#define STEPPER_GRIPPER_DIR_PORT    DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_gripper), dir_gpios))
#define STEPPER_GRIPPER_DIR_PIN     DT_GPIO_PIN(DT_NODELABEL(stepper_gripper), dir_gpios)
#define STEPPER_GRIPPER_ENABLE_PORT DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(stepper_gripper), enable_gpios))
#define STEPPER_GRIPPER_ENABLE_PIN  DT_GPIO_PIN(DT_NODELABEL(stepper_gripper), enable_gpios)
#define STEPPER_GRIPPER_HAS_ENABLE  DT_NODE_HAS_PROP(DT_NODELABEL(stepper_gripper), enable_gpios)
#define STEPPER_GRIPPER_DIR_INV     DT_NODE_HAS_PROP(DT_NODELABEL(stepper_gripper), dir_inverted)
#define STEPPER_GRIPPER_MAX_SPEED   DT_PROP_OR(DT_NODELABEL(stepper_gripper), max_speed_hz, 5000)

// Speed Config
#define STEPPER_DEFAULT_SPEED_US 1000   /* 1000 Hz = 1 ms per step */
#define STEPPER_FAST_SPEED_US    200    /* 5000 Hz = 200 µs per step */
#define STEPPER_SLOW_SPEED_US    2000   /* 500 Hz = 2 ms per step */
#define STEPPER_HOMING_SPEED_US  2000

#define STEPPER_MIN_PULSE_WIDTH_NS 5000

#endif /* STEPPER_CONFIG_H */
