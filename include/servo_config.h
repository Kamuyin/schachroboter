#ifndef SERVO_CONFIG_H
#define SERVO_CONFIG_H

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/sys/util.h>

BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(servo_1), okay), "servo_1 DT node required");

#define GRIPPER_SERVO_GPIO_PORT DT_GPIO_CTLR(DT_NODELABEL(servo_1), control_gpios)
#define GRIPPER_SERVO_GPIO_PIN  DT_GPIO_PIN (DT_NODELABEL(servo_1), control_gpios)

#endif
