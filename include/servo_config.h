#ifndef SERVO_CONFIG_H
#define SERVO_CONFIG_H

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/sys/util.h>

BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(servo_1), okay), "servo_1 DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(servo_2), okay), "servo_2 DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(servo_3), okay), "servo_3 DT node required");
BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(servo_4), okay), "servo_4 DT node required");

#define SERVO_1_GPIO_PORT DT_GPIO_CTLR(DT_NODELABEL(servo_1), control_gpios)
#define SERVO_1_GPIO_PIN  DT_GPIO_PIN (DT_NODELABEL(servo_1), control_gpios)

#define SERVO_2_GPIO_PORT DT_GPIO_CTLR(DT_NODELABEL(servo_2), control_gpios)
#define SERVO_2_GPIO_PIN  DT_GPIO_PIN (DT_NODELABEL(servo_2), control_gpios)

#define SERVO_3_GPIO_PORT DT_GPIO_CTLR(DT_NODELABEL(servo_3), control_gpios)
#define SERVO_3_GPIO_PIN  DT_GPIO_PIN (DT_NODELABEL(servo_3), control_gpios)

#define SERVO_4_GPIO_PORT DT_GPIO_CTLR(DT_NODELABEL(servo_4), control_gpios)
#define SERVO_4_GPIO_PIN  DT_GPIO_PIN (DT_NODELABEL(servo_4), control_gpios)

#endif
