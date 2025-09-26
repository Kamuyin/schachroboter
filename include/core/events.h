#pragma once
#include <zephyr/kernel.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EV_MATRIX_FRAME,     // payload: frame[8]
    EV_MOVE_CMD,         // payload: move_cmd_t
    EV_MOTION_DONE,      // payload: none
    EV_NET_UP,           // payload: none
    EV_NET_DOWN,         // payload: none
    EV_MQTT_CMD,         // payload: parsed command from MQTT (move/home/etc.)
    EV_ERROR             // payload: int err
} event_type_t;

typedef struct {
    uint8_t y; // row 0..7
    uint8_t x; // col 0..7
} square_t;

typedef struct {
    square_t from;
    square_t to;
} move_t;

typedef struct {
    float x, y, z;
} pos_t;

typedef struct {
    event_type_t type;
    int64_t ts;
    union {
        uint8_t frame[8]; // EV_MATRIX_FRAME
        move_t move;      // EV_MOVE_CMD or result of board diff
        int err;          // EV_ERROR
        // extend for MQTT command types as needed
    } u;
} event_t;

// Simple global queue-based bus for low traffic
int events_init(void);
int events_post(const event_t *ev, k_timeout_t timeout);
int events_get(event_t *out, k_timeout_t timeout);

#ifdef __cplusplus
}
#endif