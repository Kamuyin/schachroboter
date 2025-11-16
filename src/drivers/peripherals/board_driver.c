#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "board_driver.h"
#include "board_config.h"

LOG_MODULE_REGISTER(board_driver, LOG_LEVEL_INF);

typedef struct {
    const struct device *port;
    gpio_pin_t pin;
} board_gpio_pin_t;

static board_gpio_pin_t row_pins[BOARD_ROWS];
static board_gpio_pin_t col_pins[BOARD_COLS];

int board_driver_init(void)
{
    int ret;

    row_pins[0] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_0_PORT), BOARD_ROW_0_PIN};
    row_pins[1] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_1_PORT), BOARD_ROW_1_PIN};
    row_pins[2] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_2_PORT), BOARD_ROW_2_PIN};
    row_pins[3] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_3_PORT), BOARD_ROW_3_PIN};
    row_pins[4] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_4_PORT), BOARD_ROW_4_PIN};
    row_pins[5] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_5_PORT), BOARD_ROW_5_PIN};
    row_pins[6] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_6_PORT), BOARD_ROW_6_PIN};
    row_pins[7] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_ROW_7_PORT), BOARD_ROW_7_PIN};

    col_pins[0] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_0_PORT), BOARD_COL_0_PIN};
    col_pins[1] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_1_PORT), BOARD_COL_1_PIN};
    col_pins[2] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_2_PORT), BOARD_COL_2_PIN};
    col_pins[3] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_3_PORT), BOARD_COL_3_PIN};
    col_pins[4] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_4_PORT), BOARD_COL_4_PIN};
    col_pins[5] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_5_PORT), BOARD_COL_5_PIN};
    col_pins[6] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_6_PORT), BOARD_COL_6_PIN};
    col_pins[7] = (board_gpio_pin_t){DEVICE_DT_GET(BOARD_COL_7_PORT), BOARD_COL_7_PIN};

    for (int i = 0; i < BOARD_ROWS; i++) {
        if (!device_is_ready(row_pins[i].port)) {
            LOG_ERR("Row %d GPIO port not ready", i);
            return -ENODEV;
        }
        ret = gpio_pin_configure(row_pins[i].port, row_pins[i].pin, GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure row pin %d: %d", i, ret);
            return ret;
        }
    }

    for (int i = 0; i < BOARD_COLS; i++) {
        if (!device_is_ready(col_pins[i].port)) {
            LOG_ERR("Col %d GPIO port not ready", i);
            return -ENODEV;
        }
        ret = gpio_pin_configure(col_pins[i].port, col_pins[i].pin, GPIO_INPUT | GPIO_PULL_DOWN);
        if (ret < 0) {
            LOG_ERR("Failed to configure col pin %d: %d", i, ret);
            return ret;
        }
    }

    LOG_INF("Board driver initialized");
    return 0;
}

int board_driver_scan(uint64_t *board_state)
{
    int ret;
    uint64_t state = 0;

    if (!board_state) {
        return -EINVAL;
    }

    for (int row = 0; row < BOARD_ROWS; row++) {
        ret = gpio_pin_set(row_pins[row].port, row_pins[row].pin, 1);
        if (ret < 0) {
            LOG_ERR("Failed to set row %d high: %d", row, ret);
            return ret;
        }

        k_sleep(K_USEC(100));

        for (int col = 0; col < BOARD_COLS; col++) {
            int value = gpio_pin_get(col_pins[col].port, col_pins[col].pin);
            if (value < 0) {
                LOG_ERR("Failed to read col %d: %d", col, value);
                gpio_pin_set(row_pins[row].port, row_pins[row].pin, 0);
                return value;
            }

            if (value) {
                state |= (1ULL << (row * BOARD_COLS + col));
            }
        }

        ret = gpio_pin_set(row_pins[row].port, row_pins[row].pin, 0);
        if (ret < 0) {
            LOG_ERR("Failed to set row %d low: %d", row, ret);
            return ret;
        }

        k_sleep(K_MSEC(BOARD_SCAN_DELAY_MS));
    }

    *board_state = state;
    return 0;
}
