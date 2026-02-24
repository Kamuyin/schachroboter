#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "movement_planner.h"
#include "robot_controller.h"
#include "stepper_manager.h"
#include "robot_config.h"

LOG_MODULE_REGISTER(movement_planner, LOG_LEVEL_INF);

static inline int32_t file_to_x(uint8_t file)
{
    return ROBOT_CONFIG_BOARD_ORIGIN_X + (int32_t)file * ROBOT_CONFIG_STEPS_PER_SQUARE;
}

/**
 * Convert a chess rank (0=rank1 … 7=rank8) to an absolute Y step position.
 */
static inline int32_t rank_to_y(uint8_t rank)
{
    return ROBOT_CONFIG_BOARD_ORIGIN_Y + (int32_t)rank * ROBOT_CONFIG_STEPS_PER_SQUARE;
}

static void wait_xy(void)
{
    while (robot_controller_is_xy_moving()) {
        stepper_manager_update_all();
        k_usleep(100);
    }
}

/**
 * Block until the Z axis is idle.
 * Drives the stepper update loop at 100 µs intervals.
 */
static void wait_z(void)
{
    while (robot_controller_is_z_moving()) {
        stepper_manager_update_all();
        k_usleep(100);
    }
}

/* ============================================================================
 * Primitive motion sequences
 * ============================================================================ */

/**
 * @brief Pick up the piece centred on @p sq.
 *
 * Sequence:
 *   1. Move XY to the square (concurrent X + Y).
 *   2. Start Z descent AND open the gripper simultaneously.
 *   3. Wait for Z to reach ROBOT_CONFIG_Z_PICK.
 *   4. Wait GRIPPER_OPEN_DELAY_MS for the servo to fully open.
 *   5. Close the gripper.
 *   6. Wait GRIPPER_CLOSE_DELAY_MS for the servo to grip the piece.
 *   7. Raise Z to ROBOT_CONFIG_Z_TRAVEL.
 */
static int do_pickup(chess_square_t sq)
{
    int32_t x = file_to_x(sq.file);
    int32_t y = rank_to_y(sq.rank);

    LOG_INF("Pickup: moving XY to file=%u rank=%u (%d,%d steps)",
            sq.file, sq.rank, x, y);

    /* ── Step 1: XY transit ─────────────────────────────────────────────── */
    int ret = robot_controller_start_xy_move(x, y, ROBOT_CONFIG_SPEED_TRAVEL_US);
    if (ret < 0) {
        LOG_ERR("Pickup XY move failed: %d", ret);
        return ret;
    }
    wait_xy();

    /* ── Step 2: Descend Z and open gripper concurrently ────────────────── */
    ret = robot_controller_start_z_move(ROBOT_CONFIG_Z_PICK, ROBOT_CONFIG_SPEED_Z_US);
    if (ret < 0) {
        LOG_ERR("Pickup Z descend failed: %d", ret);
        return ret;
    }
    robot_controller_gripper_open(); /* fire-and-forget via servo PWM thread */

    /* ── Step 3 + 4: Wait for Z, then wait for gripper to open ─────────── */
    wait_z();
    k_msleep(ROBOT_CONFIG_GRIPPER_OPEN_DELAY_MS);

    /* ── Step 5 + 6: Close gripper and wait for grip ────────────────────── */
    robot_controller_gripper_close();
    k_msleep(ROBOT_CONFIG_GRIPPER_CLOSE_DELAY_MS);

    /* ── Step 7: Ascend to travel height ────────────────────────────────── */
    ret = robot_controller_start_z_move(ROBOT_CONFIG_Z_TRAVEL, ROBOT_CONFIG_SPEED_Z_US);
    if (ret < 0) {
        LOG_ERR("Pickup Z ascend failed: %d", ret);
        return ret;
    }
    wait_z();

    LOG_DBG("Pickup complete");
    return 0;
}

/**
 * @brief Place the currently held piece onto @p sq and release it.
 *
 * Sequence:
 *   1. Move XY to the square (concurrent X + Y).
 *   2. Descend Z to ROBOT_CONFIG_Z_PLACE (gripper remains closed).
 *   3. Open the gripper to release the piece.
 *   4. Wait GRIPPER_OPEN_DELAY_MS for the servo to open.
 *   5. Raise Z to ROBOT_CONFIG_Z_TRAVEL.
 */
static int do_place(chess_square_t sq)
{
    int32_t x = file_to_x(sq.file);
    int32_t y = rank_to_y(sq.rank);

    LOG_INF("Place: moving XY to file=%u rank=%u (%d,%d steps)",
            sq.file, sq.rank, x, y);

    /* ── Step 1: XY transit ─────────────────────────────────────────────── */
    int ret = robot_controller_start_xy_move(x, y, ROBOT_CONFIG_SPEED_TRAVEL_US);
    if (ret < 0) {
        LOG_ERR("Place XY move failed: %d", ret);
        return ret;
    }
    wait_xy();

    /* ── Step 2: Descend to place height ────────────────────────────────── */
    ret = robot_controller_start_z_move(ROBOT_CONFIG_Z_PLACE, ROBOT_CONFIG_SPEED_Z_US);
    if (ret < 0) {
        LOG_ERR("Place Z descend failed: %d", ret);
        return ret;
    }
    wait_z();

    /* ── Step 3 + 4: Release piece and wait for servo to open ───────────── */
    robot_controller_gripper_open();
    k_msleep(ROBOT_CONFIG_GRIPPER_OPEN_DELAY_MS);

    /* ── Step 5: Ascend to travel height ────────────────────────────────── */
    ret = robot_controller_start_z_move(ROBOT_CONFIG_Z_TRAVEL, ROBOT_CONFIG_SPEED_Z_US);
    if (ret < 0) {
        LOG_ERR("Place Z ascend failed: %d", ret);
        return ret;
    }
    wait_z();

    LOG_DBG("Place complete");
    return 0;
}

/**
 * @brief Place the currently held piece in the graveyard and release it.
 *
 * The graveyard position is defined by ROBOT_CONFIG_GRAVEYARD_X/Y.
 * This function is identical to do_place() but targets the fixed
 * graveyard coordinates rather than a board square.
 */
static int do_place_at_graveyard(void)
{
    LOG_INF("Placing piece at graveyard (%d,%d steps)",
            ROBOT_CONFIG_GRAVEYARD_X, ROBOT_CONFIG_GRAVEYARD_Y);

    /* ── Step 1: XY transit to graveyard ───────────────────────────────── */
    int ret = robot_controller_start_xy_move(
        ROBOT_CONFIG_GRAVEYARD_X,
        ROBOT_CONFIG_GRAVEYARD_Y,
        ROBOT_CONFIG_SPEED_TRAVEL_US);
    if (ret < 0) {
        LOG_ERR("Graveyard XY move failed: %d", ret);
        return ret;
    }
    wait_xy();

    /* ── Step 2: Descend to place height ────────────────────────────────── */
    ret = robot_controller_start_z_move(ROBOT_CONFIG_Z_PLACE, ROBOT_CONFIG_SPEED_Z_US);
    if (ret < 0) {
        LOG_ERR("Graveyard Z descend failed: %d", ret);
        return ret;
    }
    wait_z();

    /* ── Step 3 + 4: Release ───────────────────────────────────────────── */
    robot_controller_gripper_open();
    k_msleep(ROBOT_CONFIG_GRIPPER_OPEN_DELAY_MS);

    /* ── Step 5: Ascend ─────────────────────────────────────────────────── */
    ret = robot_controller_start_z_move(ROBOT_CONFIG_Z_TRAVEL, ROBOT_CONFIG_SPEED_Z_US);
    if (ret < 0) {
        LOG_ERR("Graveyard Z ascend failed: %d", ret);
        return ret;
    }
    wait_z();

    LOG_DBG("Graveyard place complete");
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void movement_planner_init(void)
{
    LOG_INF("Movement planner initialised (steps/square=%d, origin=(%d,%d))",
            ROBOT_CONFIG_STEPS_PER_SQUARE,
            ROBOT_CONFIG_BOARD_ORIGIN_X,
            ROBOT_CONFIG_BOARD_ORIGIN_Y);
}

planner_result_t movement_planner_execute(const planner_action_t *action)
{
    if (!action) {
        return PLANNER_ERR_INVALID;
    }

    int ret;

    switch (action->type) {

    /* ── Simple move: pick from 'from', place at 'to' ─────────────────── */
    case PLANNER_ACTION_MOVE:
        LOG_INF("Planner: MOVE %c%u -> %c%u",
                'a' + action->from.file, action->from.rank + 1,
                'a' + action->to.file,   action->to.rank + 1);

        ret = do_pickup(action->from);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place(action->to);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        break;

    /* ── Capture: remove opponent piece first, then execute the move ───── */
    case PLANNER_ACTION_CAPTURE:
        LOG_INF("Planner: CAPTURE – removing piece at %c%u, then moving %c%u -> %c%u",
                'a' + action->to.file,   action->to.rank + 1,
                'a' + action->from.file, action->from.rank + 1,
                'a' + action->to.file,   action->to.rank + 1);

        /* Remove opponent piece to graveyard */
        ret = do_pickup(action->to);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place_at_graveyard();
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        /* Now execute the actual move */
        ret = do_pickup(action->from);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place(action->to);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        break;

    /* ── En passant: captured pawn is on a different square than 'to' ─── */
    case PLANNER_ACTION_EN_PASSANT:
        LOG_INF("Planner: EN_PASSANT – captured pawn at %c%u, pawn %c%u -> %c%u",
                'a' + action->captured.file, action->captured.rank + 1,
                'a' + action->from.file,     action->from.rank + 1,
                'a' + action->to.file,       action->to.rank + 1);

        /* Remove the en-passant captured pawn */
        ret = do_pickup(action->captured);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place_at_graveyard();
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        /* Move the capturing pawn */
        ret = do_pickup(action->from);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place(action->to);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        break;

    /* ── Castling: rook moves first, then king ───────────────────────────
     *   from / to  → rook source / destination
     *   from2 / to2 → king source / destination                           */
    case PLANNER_ACTION_CASTLE:
        LOG_INF("Planner: CASTLE – rook %c%u->%c%u, king %c%u->%c%u",
                'a' + action->from.file,  action->from.rank + 1,
                'a' + action->to.file,    action->to.rank + 1,
                'a' + action->from2.file, action->from2.rank + 1,
                'a' + action->to2.file,   action->to2.rank + 1);

        /* Move rook first so it vacates its square before the king passes */
        ret = do_pickup(action->from);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place(action->to);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        /* Move king */
        ret = do_pickup(action->from2);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place(action->to2);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        break;

    /* ── Remove: pick a piece and deposit it in the graveyard ───────────── */
    case PLANNER_ACTION_REMOVE:
        LOG_INF("Planner: REMOVE piece at %c%u",
                'a' + action->from.file, action->from.rank + 1);

        ret = do_pickup(action->from);
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        ret = do_place_at_graveyard();
        if (ret < 0) {
            return PLANNER_ERR_MOTOR;
        }
        break;

    default:
        LOG_ERR("Unknown planner action type: %d", (int)action->type);
        return PLANNER_ERR_INVALID;
    }

    LOG_INF("Planner: action complete");
    return PLANNER_OK;
}

int movement_planner_parse_square(const char *str, chess_square_t *out)
{
    if (!str || !out) {
        return -1;
    }

    /* Need at least two characters: file letter + rank digit */
    if (str[0] == '\0' || str[1] == '\0') {
        return -1;
    }

    char file_ch = str[0];
    char rank_ch = str[1];

    /* Accept both lower-case ('a'-'h') and upper-case ('A'-'H') */
    if (file_ch >= 'A' && file_ch <= 'H') {
        file_ch = file_ch - 'A' + 'a';
    }

    if (file_ch < 'a' || file_ch > 'h') {
        LOG_WRN("Invalid file character: '%c'", str[0]);
        return -1;
    }

    if (rank_ch < '1' || rank_ch > '8') {
        LOG_WRN("Invalid rank character: '%c'", str[1]);
        return -1;
    }

    out->file = (uint8_t)(file_ch - 'a');
    out->rank = (uint8_t)(rank_ch - '1');
    return 0;
}
