#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

/** Steps required to travel 1 mm. (1 step / 0.05 mm = 20 steps/mm) */
#define ROBOT_CONFIG_STEPS_PER_MM       20

/** Side length of one chess square in millimetres. */
#define ROBOT_CONFIG_SQUARE_SIZE_MM     70

/**
 * Steps required to cross one chess square.
 */
#define ROBOT_CONFIG_STEPS_PER_SQUARE \
    (ROBOT_CONFIG_STEPS_PER_MM * ROBOT_CONFIG_SQUARE_SIZE_MM)

/**
 * Board origin offset.
 *
 * Number of steps from the homed position (0, 0) to the centre of
 * square a1 (file 0, rank 0).  Increase to push the origin further
 * away from the home switches.
 */
#define ROBOT_CONFIG_BOARD_ORIGIN_X     0
#define ROBOT_CONFIG_BOARD_ORIGIN_Y     0

#define ROBOT_CONFIG_Z_TRAVEL           0

#define ROBOT_CONFIG_Z_PICK             2000

#define ROBOT_CONFIG_Z_PLACE            1900

/**
 * Absolute step coordinates of the graveyard drop-off point.
 * Place this zone outside the board boundaries.
 * Default: to the left of a1 (-500 steps in X, aligned with rank origin).
 */
#define ROBOT_CONFIG_GRAVEYARD_X        (-500)
#define ROBOT_CONFIG_GRAVEYARD_Y        0

/** XY transit speed used while carrying or repositioning. */
#define ROBOT_CONFIG_SPEED_TRAVEL_US    800

/** Z descent / ascent speed.  Slower than XY for safety. */
#define ROBOT_CONFIG_SPEED_Z_US         1200

/**
 * Milliseconds to wait after issuing a gripper-open command before
 * attempting to close (pick) or ascend (place).  Accounts for servo
 * travel time.
 */
#define ROBOT_CONFIG_GRIPPER_OPEN_DELAY_MS    200

/**
 * Milliseconds to wait after issuing a gripper-close command before
 * continuing.  Allows the servo to fully close and grip the piece.
 */
#define ROBOT_CONFIG_GRIPPER_CLOSE_DELAY_MS   300

#endif /* ROBOT_CONFIG_H */
