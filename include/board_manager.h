#ifndef BOARD_MANAGER_H
#define BOARD_MANAGER_H

#include "board_state.h"

typedef void (*board_move_callback_t)(const board_move_t *move);
typedef void (*board_state_callback_t)(const chess_board_state_t *state);

int board_manager_init(void);
int board_manager_update(void);
const chess_board_state_t *board_manager_get_state(void);
void board_manager_register_move_callback(board_move_callback_t callback);
void board_manager_register_state_callback(board_state_callback_t callback);

#endif
