#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <cJSON.h>
#include "board_manager.h"
#include "mqtt_client.h"

LOG_MODULE_REGISTER(application, LOG_LEVEL_INF);

#define BOARD_SCAN_INTERVAL_MS 100

static void on_move_detected(const board_move_t *move)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERR("Failed to create JSON object");
        return;
    }

    cJSON_AddStringToObject(root, "type", "move");

    cJSON *from = cJSON_CreateObject();
    cJSON_AddNumberToObject(from, "row", move->from.row);
    cJSON_AddNumberToObject(from, "col", move->from.col);
    cJSON_AddItemToObject(root, "from", from);

    cJSON *to = cJSON_CreateObject();
    cJSON_AddNumberToObject(to, "row", move->to.row);
    cJSON_AddNumberToObject(to, "col", move->to.col);
    cJSON_AddItemToObject(root, "to", to);

    cJSON_AddNumberToObject(root, "timestamp", move->timestamp);

    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        int rc = app_mqtt_publish("chess/board/move", payload, strlen(payload));
        if (rc < 0) {
            LOG_WRN("Failed to publish move (rc=%d) - MQTT connected: %s", rc, app_mqtt_is_connected() ? "yes" : "no");
        } else {
            LOG_INF("Published move to MQTT");
        }
        cJSON_free(payload);
    }

    cJSON_Delete(root);
}

static void on_state_changed(const chess_board_state_t *state)
{
    // Publish compact state (hex mask, move count, timestamp)
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERR("Failed to create JSON object");
        return;
    }

    cJSON_AddStringToObject(root, "type", "state");

    char mask_str[20];
    snprintf(mask_str, sizeof(mask_str), "0x%016llx", state->occupied_mask);
    cJSON_AddStringToObject(root, "occupied", mask_str);

    cJSON_AddNumberToObject(root, "moves", state->move_count);
    cJSON_AddNumberToObject(root, "timestamp", state->last_update_time);

    char *payload = cJSON_PrintUnformatted(root);
    if (payload) {
        int rc = app_mqtt_publish("chess/board/state", payload, strlen(payload));
        if (rc < 0) {
            LOG_DBG("Failed to publish state (rc=%d) - MQTT connected: %s", rc, app_mqtt_is_connected() ? "yes" : "no");
        }
        cJSON_free(payload);
    }
    cJSON_Delete(root);

    // Publish full board grid as array to chess/board/fullstate
    cJSON *full = cJSON_CreateObject();
    if (!full) {
        LOG_ERR("Failed to create fullstate JSON object");
        return;
    }
    cJSON_AddStringToObject(full, "type", "fullstate");
    cJSON_AddNumberToObject(full, "timestamp", state->last_update_time);
    cJSON *rows = cJSON_CreateArray();
    for (int r = 0; r < CHESS_BOARD_SIZE; r++) {
        cJSON *row = cJSON_CreateArray();
        for (int c = 0; c < CHESS_BOARD_SIZE; c++) {
            int occ = (state->occupied_mask & (1ULL << (r * CHESS_BOARD_SIZE + c))) ? 1 : 0;
            cJSON_AddItemToArray(row, cJSON_CreateNumber(occ));
        }
        cJSON_AddItemToArray(rows, row);
    }
    cJSON_AddItemToObject(full, "board", rows);
    char *full_payload = cJSON_PrintUnformatted(full);
    if (full_payload) {
        int rc = app_mqtt_publish("chess/board/fullstate", full_payload, strlen(full_payload));
        if (rc < 0) {
            LOG_DBG("Failed to publish fullstate (rc=%d) - MQTT connected: %s", rc, app_mqtt_is_connected() ? "yes" : "no");
        }
        cJSON_free(full_payload);
    }
    cJSON_Delete(full);
}

static void on_ping_received(const char *topic, const uint8_t *payload, uint32_t payload_len)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LOG_ERR("Failed to create JSON object");
        return;
    }

    cJSON_AddStringToObject(root, "status", "pong");
    cJSON_AddNumberToObject(root, "timestamp", k_uptime_get_32());

    char *response = cJSON_PrintUnformatted(root);
    if (response) {
        int rc = app_mqtt_publish("chess/system/pong", response, strlen(response));
        if (rc < 0) {
            LOG_WRN("Failed to publish pong (rc=%d) - MQTT connected: %s", rc, app_mqtt_is_connected() ? "yes" : "no");
        } else {
            LOG_INF("Responded to ping");
        }
        cJSON_free(response);
    }

    cJSON_Delete(root);
}

int application_init(void)
{
    int ret;

    LOG_INF("Initializing chess board application");

    ret = board_manager_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize board manager: %d", ret);
        return ret;
    }

    board_manager_register_move_callback(on_move_detected);
    board_manager_register_state_callback(on_state_changed);

    app_mqtt_subscribe("chess/system/ping", on_ping_received);

    LOG_INF("Application initialized");
    return 0;
}

void application_task(void)
{
    while (1) {
        board_manager_update();
        k_sleep(K_MSEC(BOARD_SCAN_INTERVAL_MS));
    }
}
