#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "hal/gpio_matrix.h"
#include "core/events.h"

LOG_MODULE_REGISTER(gpio_matrix, LOG_LEVEL_INF);

static matrix_cfg_t g_cfg = {0};
static uint8_t g_last_frame[8];
static struct k_thread scan_thread;
static K_THREAD_STACK_DEFINE(scan_stack, 1024);

static void set_row_all(int val) {
    for (int r = 0; r < 8; ++r) {
        if (g_cfg.rows[r].port) {
            gpio_pin_set_dt(&g_cfg.rows[r], val);
        }
    }
}

static void scan_once(void) {
    uint8_t frame[8] = {0};
    // Typical pattern: drive one row active, read all cols
    for (int r = 0; r < 8; ++r) {
        // drive current row active, others inactive
        for (int i = 0; i < 8; ++i) {
            gpio_pin_set_dt(&g_cfg.rows[i], (i == r) ? (g_cfg.active_high ? 1 : 0) : (g_cfg.active_high ? 0 : 1));
        }
        // settle a bit
        k_busy_wait(50); // us, adjust per hardware

        for (int c = 0; c < 8; ++c) {
            int v = gpio_pin_get_dt(&g_cfg.cols[c]);
            if (v < 0) v = 0;
            if (!g_cfg.active_high) v = !v;
            if (v) frame[r] |= (1u << c);
        }
    }
    // all rows inactive
    set_row_all(g_cfg.active_high ? 0 : 1);

    // publish event if changed
    bool changed = false;
    for (int i = 0; i < 8; ++i) {
        if (frame[i] != g_last_frame[i]) { changed = true; break; }
    }
    memcpy(g_last_frame, frame, sizeof(frame));

    if (changed) {
        event_t ev = {.type = EV_MATRIX_FRAME, .ts = k_uptime_get()};
        memcpy(ev.u.frame, frame, 8);
        (void)events_post(&ev, K_NO_WAIT);
    }
}

static void scan_entry(void *a, void *b, void *c) {
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    while (1) {
        scan_once();
        k_msleep(g_cfg.period_ms);
    }
}

int gpio_matrix_init(const matrix_cfg_t *cfg) {
    memcpy(&g_cfg, cfg, sizeof(matrix_cfg_t));
    for (int r = 0; r < 8; ++r) {
        if (!device_is_ready(g_cfg.rows[r].port)) return -ENODEV;
        int flags = g_cfg.active_high ? GPIO_OUTPUT_INACTIVE : GPIO_OUTPUT_ACTIVE;
        int rc = gpio_pin_configure_dt(&g_cfg.rows[r], GPIO_OUTPUT | flags);
        if (rc) return rc;
    }
    for (int c = 0; c < 8; ++c) {
        if (!device_is_ready(g_cfg.cols[c].port)) return -ENODEV;
        int rc = gpio_pin_configure_dt(&g_cfg.cols[c], GPIO_INPUT | GPIO_PULL_UP);
        if (rc) return rc;
    }
    memset(g_last_frame, 0, sizeof(g_last_frame));
    set_row_all(g_cfg.active_high ? 0 : 1);
    return 0;
}

int gpio_matrix_start(void) {
    k_thread_create(&scan_thread, scan_stack, K_THREAD_STACK_SIZEOF(scan_stack),
                    scan_entry, NULL, NULL, NULL,
                    K_PRIO_COOP(10), 0, K_NO_WAIT);
    k_thread_name_set(&scan_thread, "matrix_scan");
    return 0;
}

void gpio_matrix_get_last(uint8_t out_frame[8]) {
    memcpy(out_frame, g_last_frame, 8);
}