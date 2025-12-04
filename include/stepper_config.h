#ifndef STEPPER_CONFIG_H
#define STEPPER_CONFIG_H

// X-Axis Stepper: Relocated to PD pins (avoiding Ethernet PA1, PA2, PA7)
#define STEPPER_X_PULSE_PORT DT_NODELABEL(gpiod)
#define STEPPER_X_PULSE_PIN 2  // Previously PA0
#define STEPPER_X_DIR_PORT DT_NODELABEL(gpiod)
#define STEPPER_X_DIR_PIN 3    // Previously PA1 - CONFLICT with RMII_REF_CLK!
#define STEPPER_X_ENABLE_PORT DT_NODELABEL(gpiod)
#define STEPPER_X_ENABLE_PIN 4  // Previously PA4

// Y-Axis Stepper: PB0-2 are safe (no Ethernet conflicts except PB13)
#define STEPPER_Y_PULSE_PORT DT_NODELABEL(gpiob)
#define STEPPER_Y_PULSE_PIN 3   // Previously PB0 - Changed to avoid potential issues
#define STEPPER_Y_DIR_PORT DT_NODELABEL(gpiob)
#define STEPPER_Y_DIR_PIN 4     // Previously PB1
#define STEPPER_Y_ENABLE_PORT DT_NODELABEL(gpiob)
#define STEPPER_Y_ENABLE_PIN 5  // Previously PB2

// Z-Axis Stepper: Relocated to PD pins (avoiding Ethernet PC1, PC4, PC5)
#define STEPPER_Z_PULSE_PORT DT_NODELABEL(gpiod)
#define STEPPER_Z_PULSE_PIN 5   // Previously PC0
#define STEPPER_Z_DIR_PORT DT_NODELABEL(gpiod)
#define STEPPER_Z_DIR_PIN 6     // Previously PC1 - CONFLICT with RMII_MDC!
#define STEPPER_Z_ENABLE_PORT DT_NODELABEL(gpiod)
#define STEPPER_Z_ENABLE_PIN 7  // Previously PC2

// Gripper Stepper: Relocated to PC pins that are safe
#define STEPPER_GRIPPER_PULSE_PORT DT_NODELABEL(gpioc)
#define STEPPER_GRIPPER_PULSE_PIN 6  // Previously PC3 - Safe
#define STEPPER_GRIPPER_DIR_PORT DT_NODELABEL(gpioc)
#define STEPPER_GRIPPER_DIR_PIN 7    // Previously PC4 - CONFLICT with RMII_RXD0!
#define STEPPER_GRIPPER_ENABLE_PORT DT_NODELABEL(gpioc)
#define STEPPER_GRIPPER_ENABLE_PIN 8  // Previously PC5 - CONFLICT with RMII_RXD1!

#define STEPPER_DEFAULT_SPEED_US 1000
#define STEPPER_FAST_SPEED_US 500
#define STEPPER_SLOW_SPEED_US 2000

#endif
