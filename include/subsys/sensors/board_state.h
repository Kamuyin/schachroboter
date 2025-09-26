#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "core/events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t stable[8];
    uint8_t accum_same[8];
    uint8_t threshold; // number of consecutive identical frames to accept
} board_state_t;

void board_state_init(board_state_t *s, uint8_t threshold);
bool board_state_update(board_state_t *s, const uint8_t frame[8]); // returns true if stable changed
void board_state_copy(const board_state_t *s, uint8_t out[8]);
bool board_state_detect_move(const uint8_t prev[8], const uint8_t curr[8], move_t *out);

#ifdef __cplusplus
}
#endif