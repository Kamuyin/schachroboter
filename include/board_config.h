#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// --- Rows ---
#define BOARD_ROW_0_PORT    DT_NODELABEL(gpiof) //5
#define BOARD_ROW_0_PIN     1
#define BOARD_ROW_1_PORT    DT_NODELABEL(gpioe) //7
#define BOARD_ROW_1_PIN     4
#define BOARD_ROW_2_PORT    DT_NODELABEL(gpiof) //9
#define BOARD_ROW_2_PIN     0
#define BOARD_ROW_3_PORT    DT_NODELABEL(gpioe) 
#define BOARD_ROW_3_PIN     5
#define BOARD_ROW_4_PORT    DT_NODELABEL(gpiod)
#define BOARD_ROW_4_PIN     1
#define BOARD_ROW_5_PORT    DT_NODELABEL(gpiof)
#define BOARD_ROW_5_PIN     2 
#define BOARD_ROW_6_PORT    DT_NODELABEL(gpiod)
#define BOARD_ROW_6_PIN     0
#define BOARD_ROW_7_PORT    DT_NODELABEL(gpiof)
#define BOARD_ROW_7_PIN     8

// --- Columns ---
#define BOARD_COL_0_PORT    DT_NODELABEL(gpiog)
#define BOARD_COL_0_PIN     0
#define BOARD_COL_1_PORT    DT_NODELABEL(gpiof)
#define BOARD_COL_1_PIN     9
#define BOARD_COL_2_PORT    DT_NODELABEL(gpioe)
#define BOARD_COL_2_PIN     1
#define BOARD_COL_3_PORT    DT_NODELABEL(gpiog)
#define BOARD_COL_3_PIN     1
#define BOARD_COL_4_PORT    DT_NODELABEL(gpiog)
#define BOARD_COL_4_PIN     9
#define BOARD_COL_5_PORT    DT_NODELABEL(gpioe)
#define BOARD_COL_5_PIN     6
#define BOARD_COL_6_PORT    DT_NODELABEL(gpiog)
#define BOARD_COL_6_PIN     12
#define BOARD_COL_7_PORT    DT_NODELABEL(gpiog)
#define BOARD_COL_7_PIN     15

#define BOARD_ROWS          8
#define BOARD_COLS          8
#define BOARD_SCAN_DELAY_MS 10

/*
 * BOARD_INPUT_PULL_UP:
 *   0 -> Configure column inputs with internal pull-down (default)
 *   1 -> Configure column inputs with internal pull-up
 *
 * BOARD_SWITCH_ACTIVE_HIGH:
 *   1 -> A closed reed switch makes the column read HIGH when its row is driven HIGH (default)
 *   0 -> A closed reed switch makes the column read LOW (active-low wiring)
 */
#ifndef BOARD_INPUT_PULL_UP
#define BOARD_INPUT_PULL_UP 0
#endif

#ifndef BOARD_SWITCH_ACTIVE_HIGH
#define BOARD_SWITCH_ACTIVE_HIGH 1
#endif

#endif
