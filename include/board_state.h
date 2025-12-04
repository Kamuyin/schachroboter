#ifndef BOARD_STATE_H
#define BOARD_STATE_H

#include <stdint.h>
#include <stdbool.h>

#define CHESS_BOARD_SIZE 8

typedef enum
{
    SQUARE_EMPTY = 0,
    SQUARE_OCCUPIED = 1
} square_state_t;

typedef struct
{
    uint8_t row;
    uint8_t col;
} board_position_t;

typedef struct
{
    board_position_t from;
    board_position_t to;
    uint32_t timestamp;
} board_move_t;

typedef struct
{
    uint64_t occupied_mask;
    uint64_t previous_mask;
    uint32_t last_update_time;
    uint32_t move_count;
} chess_board_state_t;

static inline bool is_square_occupied(uint64_t mask, uint8_t row, uint8_t col)
{
    if (row >= CHESS_BOARD_SIZE || col >= CHESS_BOARD_SIZE)
    {
        return false;
    }
    return (mask & (1ULL << (row * CHESS_BOARD_SIZE + col))) != 0;
}

static inline void set_square(uint64_t *mask, uint8_t row, uint8_t col, bool occupied)
{
    if (row >= CHESS_BOARD_SIZE || col >= CHESS_BOARD_SIZE || !mask)
    {
        return;
    }
    if (occupied)
    {
        *mask |= (1ULL << (row * CHESS_BOARD_SIZE + col));
    }
    else
    {
        *mask &= ~(1ULL << (row * CHESS_BOARD_SIZE + col));
    }
}

#endif
