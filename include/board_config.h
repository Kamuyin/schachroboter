#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define BOARD_ROW_0_PORT    DT_NODELABEL(gpiod)
#define BOARD_ROW_0_PIN     13
#define BOARD_ROW_1_PORT    DT_NODELABEL(gpiod)
#define BOARD_ROW_1_PIN     12
#define BOARD_ROW_2_PORT    DT_NODELABEL(gpiod)
#define BOARD_ROW_2_PIN     11
#define BOARD_ROW_3_PORT    DT_NODELABEL(gpioe)
#define BOARD_ROW_3_PIN     10
#define BOARD_ROW_4_PORT    DT_NODELABEL(gpioe)
#define BOARD_ROW_4_PIN     12
#define BOARD_ROW_5_PORT    DT_NODELABEL(gpioe)
#define BOARD_ROW_5_PIN     14
#define BOARD_ROW_6_PORT    DT_NODELABEL(gpioe)
#define BOARD_ROW_6_PIN     15
#define BOARD_ROW_7_PORT    DT_NODELABEL(gpioe)
#define BOARD_ROW_7_PIN     13

#define BOARD_COL_0_PORT    DT_NODELABEL(gpiof)
#define BOARD_COL_0_PIN     4
#define BOARD_COL_1_PORT    DT_NODELABEL(gpioe)
#define BOARD_COL_1_PIN     8
#define BOARD_COL_2_PORT    DT_NODELABEL(gpiof)
#define BOARD_COL_2_PIN     10
#define BOARD_COL_3_PORT    DT_NODELABEL(gpioe)
#define BOARD_COL_3_PIN     7
#define BOARD_COL_4_PORT    DT_NODELABEL(gpiod)
#define BOARD_COL_4_PIN     14
#define BOARD_COL_5_PORT    DT_NODELABEL(gpiod)
#define BOARD_COL_5_PIN     15
#define BOARD_COL_6_PORT    DT_NODELABEL(gpiof)
#define BOARD_COL_6_PIN     14
#define BOARD_COL_7_PORT    DT_NODELABEL(gpioe)
#define BOARD_COL_7_PIN     9

#define BOARD_ROWS          8
#define BOARD_COLS          8
#define BOARD_ACTIVE_HIGH   1
#define BOARD_SCAN_DELAY_MS 10

#endif
