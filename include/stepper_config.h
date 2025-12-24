#ifndef STEPPER_CONFIG_H
#define STEPPER_CONFIG_H

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/sys/util.h>

BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_x), okay), "stepper_x DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_y1), okay), "stepper_y1 DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_y2), okay), "stepper_y2 DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_z), okay), "stepper_z DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(stepper_gripper), okay), "stepper_gripper DT node required");

/* X-Axis */
#define STEPPER_X_PULSE_PORT  DT_GPIO_CTLR(DT_NODELABEL(stepper_x), pulse_gpios)
#define STEPPER_X_PULSE_PIN   DT_GPIO_PIN (DT_NODELABEL(stepper_x), pulse_gpios)
#define STEPPER_X_DIR_PORT    DT_GPIO_CTLR(DT_NODELABEL(stepper_x), dir_gpios)
#define STEPPER_X_DIR_PIN     DT_GPIO_PIN (DT_NODELABEL(stepper_x), dir_gpios)
#define STEPPER_X_ENABLE_PORT DT_GPIO_CTLR(DT_NODELABEL(stepper_x), enable_gpios)
#define STEPPER_X_ENABLE_PIN  DT_GPIO_PIN (DT_NODELABEL(stepper_x), enable_gpios)

/* Y-Axis (Left rail) */
#define STEPPER_Y1_PULSE_PORT  DT_GPIO_CTLR(DT_NODELABEL(stepper_y1), pulse_gpios)
#define STEPPER_Y1_PULSE_PIN   DT_GPIO_PIN (DT_NODELABEL(stepper_y1), pulse_gpios)
#define STEPPER_Y1_DIR_PORT    DT_GPIO_CTLR(DT_NODELABEL(stepper_y1), dir_gpios)
#define STEPPER_Y1_DIR_PIN     DT_GPIO_PIN (DT_NODELABEL(stepper_y1), dir_gpios)
#define STEPPER_Y1_ENABLE_PORT DT_GPIO_CTLR(DT_NODELABEL(stepper_y1), enable_gpios)
#define STEPPER_Y1_ENABLE_PIN  DT_GPIO_PIN (DT_NODELABEL(stepper_y1), enable_gpios)

/* Y-Axis (Right rail) */
#define STEPPER_Y2_PULSE_PORT  DT_GPIO_CTLR(DT_NODELABEL(stepper_y2), pulse_gpios)
#define STEPPER_Y2_PULSE_PIN   DT_GPIO_PIN (DT_NODELABEL(stepper_y2), pulse_gpios)
#define STEPPER_Y2_DIR_PORT    DT_GPIO_CTLR(DT_NODELABEL(stepper_y2), dir_gpios)
#define STEPPER_Y2_DIR_PIN     DT_GPIO_PIN (DT_NODELABEL(stepper_y2), dir_gpios)
#define STEPPER_Y2_ENABLE_PORT DT_GPIO_CTLR(DT_NODELABEL(stepper_y2), enable_gpios)
#define STEPPER_Y2_ENABLE_PIN  DT_GPIO_PIN (DT_NODELABEL(stepper_y2), enable_gpios)

/* Z-Axis */
#define STEPPER_Z_PULSE_PORT  DT_GPIO_CTLR(DT_NODELABEL(stepper_z), pulse_gpios)
#define STEPPER_Z_PULSE_PIN   DT_GPIO_PIN (DT_NODELABEL(stepper_z), pulse_gpios)
#define STEPPER_Z_DIR_PORT    DT_GPIO_CTLR(DT_NODELABEL(stepper_z), dir_gpios)
#define STEPPER_Z_DIR_PIN     DT_GPIO_PIN (DT_NODELABEL(stepper_z), dir_gpios)
#define STEPPER_Z_ENABLE_PORT DT_GPIO_CTLR(DT_NODELABEL(stepper_z), enable_gpios)
#define STEPPER_Z_ENABLE_PIN  DT_GPIO_PIN (DT_NODELABEL(stepper_z), enable_gpios)

/* Gripper */
#define STEPPER_GRIPPER_PULSE_PORT  DT_GPIO_CTLR(DT_NODELABEL(stepper_gripper), pulse_gpios)
#define STEPPER_GRIPPER_PULSE_PIN   DT_GPIO_PIN (DT_NODELABEL(stepper_gripper), pulse_gpios)
#define STEPPER_GRIPPER_DIR_PORT    DT_GPIO_CTLR(DT_NODELABEL(stepper_gripper), dir_gpios)
#define STEPPER_GRIPPER_DIR_PIN     DT_GPIO_PIN (DT_NODELABEL(stepper_gripper), dir_gpios)
#define STEPPER_GRIPPER_ENABLE_PORT DT_GPIO_CTLR(DT_NODELABEL(stepper_gripper), enable_gpios)
#define STEPPER_GRIPPER_ENABLE_PIN  DT_GPIO_PIN (DT_NODELABEL(stepper_gripper), enable_gpios)

#define STEPPER_DEFAULT_SPEED_US 1000
#define STEPPER_FAST_SPEED_US 500
#define STEPPER_SLOW_SPEED_US 2000

#endif
