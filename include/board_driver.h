#ifndef BOARD_DRIVER_H
#define BOARD_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

int board_driver_init(void);
int board_driver_scan(uint64_t *board_state);

#endif
