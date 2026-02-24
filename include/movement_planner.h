#ifndef MOVEMENT_PLANNER_H
#define MOVEMENT_PLANNER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * A single square on the chessboard.
 *
 * @param file  Column: 0 = a, 1 = b, …, 7 = h
 * @param rank  Row:    0 = rank 1, 1 = rank 2, …, 7 = rank 8
 */
typedef struct {
    uint8_t file; /**< 0-7  (a-h) */
    uint8_t rank; /**< 0-7  (rank 1–8) */
} chess_square_t;

/**
 * High-level chess actions the planner can execute.
 *
 * PLANNER_ACTION_MOVE
 *   Pick the piece at @c from, place it at @c to.
 *   Used for all quiet moves and pawn promotions (the host handles the
 *   logistics of swapping piece types above board).
 *
 * PLANNER_ACTION_CAPTURE
 *   Remove the opponent piece at @c to to the graveyard, then
 *   pick the piece at @c from and place it at @c to.
 *
 * PLANNER_ACTION_EN_PASSANT
 *   Remove the captured pawn at @c captured to the graveyard, then
 *   move the capturing pawn from @c from to @c to.
 *
 * PLANNER_ACTION_CASTLE
 *   Move the rook (from → to) then the king (from2 → to2).
 *   The rook is always specified first so it vacates its square before
 *   the king crosses it.
 *
 * PLANNER_ACTION_REMOVE
 *   Pick the piece at @c from and deposit it in the graveyard.
 *   Useful for manually removing pieces during setup or correction.
 */
typedef enum {
    PLANNER_ACTION_MOVE        = 0,
    PLANNER_ACTION_CAPTURE     = 1,
    PLANNER_ACTION_EN_PASSANT  = 2,
    PLANNER_ACTION_CASTLE      = 3,
    PLANNER_ACTION_REMOVE      = 4,
} planner_action_type_t;

/**
 * Fully describes one chess action for the planner.
 *
 * Only populate the fields relevant to the chosen action type:
 *
 * | type            | required fields             |
 * |-----------------|-----------------------------|
 * | MOVE            | from, to                    |
 * | CAPTURE         | from, to                    |
 * | EN_PASSANT      | from, to, captured          |
 * | CASTLE          | from, to (rook), from2, to2 |
 * | REMOVE          | from                        |
 */
typedef struct {
    planner_action_type_t type;

    chess_square_t from;       /**< Primary piece – source square.           */
    chess_square_t to;         /**< Primary piece – destination square.      */

    chess_square_t captured;   /**< EN_PASSANT: square of the captured pawn. */

    chess_square_t from2;      /**< CASTLE: king source square.              */
    chess_square_t to2;        /**< CASTLE: king destination square.         */
} planner_action_t;

typedef enum {
    PLANNER_OK          =  0,  /**< Action completed successfully.           */
    PLANNER_ERR_BUSY    = -1,  /**< Planner is already executing an action.  */
    PLANNER_ERR_INVALID = -2,  /**< Action descriptor is malformed.          */
    PLANNER_ERR_MOTOR   = -3,  /**< A motor command returned an error.       */
} planner_result_t;

void movement_planner_init(void);


planner_result_t movement_planner_execute(const planner_action_t *action);

int movement_planner_parse_square(const char *str, chess_square_t *out);

#endif /* MOVEMENT_PLANNER_H */
