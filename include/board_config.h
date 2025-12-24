#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/sys/util.h>

#define BOARD_ROWS          8
#define BOARD_COLS          8
#define BOARD_SCAN_DELAY_MS 10

BUILD_ASSERT(DT_NODE_HAS_STATUS(DT_NODELABEL(chess_board), okay), "chess_board DT node required");

/* Rows */
#define BOARD_ROW_0_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 0)
#define BOARD_ROW_0_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 0)
#define BOARD_ROW_1_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 1)
#define BOARD_ROW_1_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 1)
#define BOARD_ROW_2_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 2)
#define BOARD_ROW_2_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 2)
#define BOARD_ROW_3_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 3)
#define BOARD_ROW_3_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 3)
#define BOARD_ROW_4_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 4)
#define BOARD_ROW_4_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 4)
#define BOARD_ROW_5_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 5)
#define BOARD_ROW_5_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 5)
#define BOARD_ROW_6_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 6)
#define BOARD_ROW_6_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 6)
#define BOARD_ROW_7_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), row_gpios, 7)
#define BOARD_ROW_7_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), row_gpios, 7)

/* Columns */
#define BOARD_COL_0_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 0)
#define BOARD_COL_0_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 0)
#define BOARD_COL_1_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 1)
#define BOARD_COL_1_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 1)
#define BOARD_COL_2_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 2)
#define BOARD_COL_2_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 2)
#define BOARD_COL_3_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 3)
#define BOARD_COL_3_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 3)
#define BOARD_COL_4_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 4)
#define BOARD_COL_4_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 4)
#define BOARD_COL_5_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 5)
#define BOARD_COL_5_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 5)
#define BOARD_COL_6_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 6)
#define BOARD_COL_6_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 6)
#define BOARD_COL_7_PORT DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(chess_board), col_gpios, 7)
#define BOARD_COL_7_PIN  DT_GPIO_PIN_BY_IDX (DT_NODELABEL(chess_board), col_gpios, 7)

#if DT_NODE_HAS_PROP(DT_NODELABEL(chess_board), pull_up)
	#define BOARD_INPUT_PULL_UP 1
#else
	#define BOARD_INPUT_PULL_UP 0
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(chess_board), switch_active_high)
	#define BOARD_SWITCH_ACTIVE_HIGH 1
#else
	#define BOARD_SWITCH_ACTIVE_HIGH 0
#endif

#endif
