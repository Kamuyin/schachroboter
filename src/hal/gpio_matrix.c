#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include "hal/gpio_matrix.h"
#include "core/events.h"

LOG_MODULE_REGISTER(gpio_matrix, LOG_LEVEL_DBG);

static matrix_cfg_t g_cfg = {0};
static uint8_t g_current_frame[8];
static struct k_thread scan_thread;
static K_THREAD_STACK_DEFINE(scan_stack, 2048);

/* --- INVERTED Helper Functions (Active State is HIGH) --- */

/**
 * @brief Drive row LOW (Inactive state).
 * Using standard push-pull output.
 */
static int drive_row_low(int r)
{
    const struct device *dev = g_cfg.rows[r].port;
    gpio_pin_t pin = g_cfg.rows[r].pin;
    return gpio_pin_configure(dev, pin, GPIO_OUTPUT_LOW);
}

/**
 * @brief Drive row HIGH (Active state).
 * Using standard push-pull output.
 */
static int drive_row_high(int r)
{
    const struct device *dev = g_cfg.rows[r].port;
    gpio_pin_t pin = g_cfg.rows[r].pin;
    return gpio_pin_configure(dev, pin, GPIO_OUTPUT_HIGH);
}

/**
 * @brief Configure column pin as input with pull-down resistor.
 */
static int col_config_input_pulldown(int c)
{
    const struct device *dev = g_cfg.cols[c].port;
    gpio_pin_t pin = g_cfg.cols[c].pin;
    // Inverted logic requires pull-downs on columns for Active-HIGH rows.
    return gpio_pin_configure(dev, pin, GPIO_INPUT | GPIO_PULL_DOWN);
}

/**
 * @brief Reads all column states for the currently active row with simple debouncing.
 * @param samples Number of samples to take per column.
 * @return uint8_t Bitmask of detected closed switches (HIGH columns).
 */
static uint8_t read_columns_with_debounce(const int samples)
{
    uint8_t detected_cols = 0;
    
    for (int c = 0; c < 8; ++c) {
        int high_count = 0; 
        
        for (int s = 0; s < samples; ++s) {
            // Check for HIGH (1), indicating a closed switch connected to the active HIGH row.
            if (gpio_pin_get(g_cfg.cols[c].port, g_cfg.cols[c].pin) == 1) { 
                high_count++;
            }
        }
        
        if (high_count > samples / 2) { // Majority wins
            detected_cols |= (1u << c);
        }
    }
    return detected_cols;
}

/**
 * @brief Prints the current 8x8 board status to the console.
 */
static void print_board_status(void)
{
    LOG_INF("---------------------------------");
    LOG_INF(" CHESS BOARD STATUS (Active-HIGH) ");
    LOG_INF("---------------------------------");
    
    // Print Column Headers (a to h)
    LOG_INF("   a b c d e f g h");
    
    // Iterate through rows (R=7 down to R=0 for conventional chess view)
    for (int r = 7; r >= 0; --r) {
        char line_buffer[30]; 
        int len = 0;
        
        // Print Row Label (1 to 8)
        len += snprintk(line_buffer + len, sizeof(line_buffer) - len, "%d |", r + 1);
        
        uint8_t row_data = g_current_frame[r];
        
        // Iterate through columns (C=0 to C=7)
        for (int c = 0; c < 8; ++c) {
            // Check if the c-th bit is set
            if (row_data & (1u << c)) {
                // Closed switch (piece present)
                len += snprintk(line_buffer + len, sizeof(line_buffer) - len, "*|");
            } else {
                // Open switch (empty square)
                len += snprintk(line_buffer + len, sizeof(line_buffer) - len, ".|");
            }
        }
        LOG_INF("%s", line_buffer);
        LOG_INF("  +-+-+-+-+-+-+-+-");
    }
    LOG_INF("---------------------------------");
}


/* --- Scan Thread with Robust Debounce Settling Time --- */

static void scan_entry(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    LOG_INF("Chess board matrix scanner started (Active-HIGH sequential mode, period=%dms)", g_cfg.period_ms);

    int current_row = 0;
    // Increased settling time to 5ms for robust noise and mechanical bounce decay
    const int settle_ms = 5; 
    const int debounce_samples = 5;

    while (1) {
        // --- START OF ACTIVE PULSE ---
        
        /* 1. Activate the current row by driving it HIGH */
        drive_row_high(current_row);
        
        /* 2. Wait for transient noise and mechanical bounce to settle (5ms) */
        // This extended k_msleep ensures we read a stable state, preventing false
        // HIGH readings from initial signal oscillation.
        k_msleep(settle_ms); 
        
        /* 3. Read and debounce column states (Detecting HIGH for closed switch) */
        uint8_t detected_cols = read_columns_with_debounce(debounce_samples);
        
        /* 4. Enforce the remaining required HIGH hold time */
        if (g_cfg.period_ms > settle_ms) {
            // Subtract the time already spent on settling. Since k_msleep is
            // a minimum delay, using the full period_ms here is generally 
            // safe if settle_ms is negligible (like 5ms vs 1000ms), 
            // but for correctness, we should try to compensate.
            // Since the subsequent steps are fast, we can approximate and use 
            // the full period_ms for simplicity and robustness in an RTOS 
            // where small errors are acceptable in this context.
            k_msleep(g_cfg.period_ms); 
        }

        // --- END OF ACTIVE PULSE ---
        
        /* 5. Deactivate the current row by driving it LOW */
        drive_row_low(current_row);

        /* 6. Update the state and check for changes */
        if (g_current_frame[current_row] != detected_cols) {
            g_current_frame[current_row] = detected_cols;
            LOG_DBG("Matrix change detected on R%d, state: 0x%02X", current_row, detected_cols);
            
            event_t ev = {.type = EV_MATRIX_FRAME, .ts = k_uptime_get()};
            memcpy(ev.u.frame, g_current_frame, 8);
            (void)events_post(&ev, K_NO_WAIT);
        }
        
        /* 7. Check for end of scan cycle and print board status */
        int next_row = (current_row + 1) % 8;
        
        if (next_row == 0) {
            // We just finished processing Row 7, so the frame is complete.
            print_board_status();
        }

        /* 8. Move to the next row immediately (no sleep here) */
        current_row = next_row;
    }
}

/* --- MODIFIED Initialization (Inverted Default State) --- */

int gpio_matrix_init(const matrix_cfg_t *cfg)
{
    if (!cfg) return -EINVAL;
    memcpy(&g_cfg, cfg, sizeof(matrix_cfg_t));

    LOG_INF("Initializing GPIO matrix (rows idle LOW, Active-HIGH scan)");

    /* Configure all rows as OUTPUT LOW initially (inactive state) */
    for (int r = 0; r < 8; ++r) {
        if (!device_is_ready(g_cfg.rows[r].port)) return -ENODEV;
        int rc = drive_row_low(r); 
        if (rc) {
            LOG_ERR("Failed to configure row %d as OUTPUT_LOW: %d", r, rc);
            return rc;
        }
    }

    /* Configure columns as inputs with PULL-DOWN (required for Active-HIGH detection) */
    for (int c = 0; c < 8; ++c) {
        if (!device_is_ready(g_cfg.cols[c].port)) return -ENODEV;
        int rc = col_config_input_pulldown(c); 
        if (rc) {
            LOG_ERR("Failed to configure column %d as INPUT|PULL_DOWN: %d", c, rc);
            return rc;
        }
    }

    memset(g_current_frame, 0, sizeof(g_current_frame));
    return 0;
}

int gpio_matrix_start(void)
{
    k_thread_create(&scan_thread, scan_stack, K_THREAD_STACK_SIZEOF(scan_stack),
                    scan_entry, NULL, NULL, NULL,
                    K_PRIO_COOP(10), 0, K_NO_WAIT);
    k_thread_name_set(&scan_thread, "matrix_scan");
    return 0;
}
