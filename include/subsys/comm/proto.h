#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "core/events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *base_topic;   // e.g. "schach/robot/1"
} proto_cfg_t;

void proto_init(const proto_cfg_t *cfg);

// topics
const char *proto_topic_status(void);   // base/status
const char *proto_topic_cmd(void);      // base/cmd

// encoders
int proto_encode_status(char *buf, size_t len, const uint8_t board[8], const pos_t *pos, int last_err);

// decoders (very naive)
typedef enum { CMD_NONE, CMD_HOME, CMD_MOVE_TO } cmd_type_t;
typedef struct {
    cmd_type_t type;
    pos_t target; // for MOVE_TO
} cmd_t;

bool proto_decode_cmd(const char *json, cmd_t *out);

#ifdef __cplusplus
}
#endif