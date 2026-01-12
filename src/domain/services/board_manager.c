#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "board_manager.h"
#include "board_driver.h"

LOG_MODULE_REGISTER(board_manager, LOG_LEVEL_INF);

static chess_board_state_t board_state;
static board_move_callback_t move_callback = NULL;
static board_state_callback_t state_callback = NULL;

// Dbg hlpr
static void log_board_mask(uint64_t mask)
{
    for (int r = 0; r < CHESS_BOARD_SIZE; r++) {
        char line[CHESS_BOARD_SIZE + 1];
        for (int c = 0; c < CHESS_BOARD_SIZE; c++) {
            bool occ = (mask & (1ULL << (r * CHESS_BOARD_SIZE + c))) != 0ULL;
            line[c] = occ ? '1' : '0';
        }
        line[CHESS_BOARD_SIZE] = '\0';
        LOG_DBG("row %d: %s", r, line);
    }
}

int board_manager_init(void)
{
    int ret;

    memset(&board_state, 0, sizeof(board_state));

    ret = board_driver_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize board driver: %d", ret);
        return ret;
    }

    ret = board_driver_scan(&board_state.occupied_mask);
    if (ret < 0) {
        LOG_ERR("Initial board scan failed: %d", ret);
        return ret;
    }

    board_state.previous_mask = board_state.occupied_mask;
    board_state.last_update_time = k_uptime_get_32();

    LOG_INF("Board manager initialized");
    return 0;
}

static void detect_and_report_move(uint64_t old_mask, uint64_t new_mask)
{
    uint64_t changed = old_mask ^ new_mask;
    uint64_t removed = old_mask & changed;
    uint64_t added = new_mask & changed;

    int removed_count = __builtin_popcountll(removed);
    int added_count = __builtin_popcountll(added);

    if (removed_count == 1 && added_count == 1) {
        board_move_t move;
        move.timestamp = k_uptime_get_32();

        for (int i = 0; i < 64; i++) {
            if (removed & (1ULL << i)) {
                move.from.row = i / CHESS_BOARD_SIZE;
                move.from.col = i % CHESS_BOARD_SIZE;
            }
            if (added & (1ULL << i)) {
                move.to.row = i / CHESS_BOARD_SIZE;
                move.to.col = i % CHESS_BOARD_SIZE;
            }
        }

        LOG_INF("Move detected: (%d,%d) -> (%d,%d)",
                move.from.row, move.from.col,
                move.to.row, move.to.col);

        if (move_callback) {
            move_callback(&move);
        }

        board_state.move_count++;
    } else if (removed_count == 2 && added_count == 2) {
        LOG_INF("Castling detected");
    } else if (removed_count == 2 && added_count == 1) {
        LOG_INF("Capture detected");
    } else if (changed != 0) {
        LOG_DBG("Complex board change detected (removed: %d, added: %d)",
                removed_count, added_count);
    }
}

int board_manager_update(void)
{
    int ret;
    uint64_t new_mask;

    ret = board_driver_scan(&new_mask);
    if (ret < 0) {
        LOG_ERR("Board scan failed: %d", ret);
        return ret;
    }

    if (new_mask != board_state.occupied_mask) {
        detect_and_report_move(board_state.occupied_mask, new_mask);

        board_state.previous_mask = board_state.occupied_mask;
        board_state.occupied_mask = new_mask;
        board_state.last_update_time = k_uptime_get_32();

        LOG_DBG("Board state changed. New mask:");
        log_board_mask(new_mask);

        if (state_callback) {
            state_callback(&board_state);
        }
    }

    return 0;
}

const chess_board_state_t *board_manager_get_state(void)
{
    return &board_state;
}

void board_manager_register_move_callback(board_move_callback_t callback)
{
    move_callback = callback;
}

void board_manager_register_state_callback(board_state_callback_t callback)
{
    state_callback = callback;
}
