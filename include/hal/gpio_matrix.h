#pragma once
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const struct gpio_dt_spec rows[8]; // driven one-at-a-time
    const struct gpio_dt_spec cols[8]; // read as inputs
    bool active_high;                  // true if magnet closes to logic 1
    uint32_t period_ms;                // scan period
} matrix_cfg_t;

int gpio_matrix_init(const matrix_cfg_t *cfg);
int gpio_matrix_start(void); // periodic scan in a worker thread
void gpio_matrix_get_last(uint8_t out_frame[8]); // snapshot (no lock)

#ifdef __cplusplus
}
#endif