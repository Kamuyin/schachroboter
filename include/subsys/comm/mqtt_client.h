#pragma once
#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include "subsys/comm/proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mqtt_cmd_cb_t)(const cmd_t *cmd);

typedef struct {
    const char *client_id;
    const char *broker_host; // IPv4 string or mDNS resolved addr
    uint16_t broker_port;
} mqttc_cfg_t;

int mqttc_init(const mqttc_cfg_t *cfg, mqtt_cmd_cb_t on_cmd);
int mqttc_start(void);
int mqttc_publish_status(const uint8_t board[8], const pos_t *pos, int last_err);

#ifdef __cplusplus
}
#endif