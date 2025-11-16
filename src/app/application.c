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
        app_mqtt_publish("chess/board/move", payload, strlen(payload));
        LOG_INF("Published move to MQTT");
        cJSON_free(payload);
    }

    cJSON_Delete(root);
}

static void on_state_changed(const chess_board_state_t *state)
{
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
        app_mqtt_publish("chess/board/state", payload, strlen(payload));
        cJSON_free(payload);
    }

    cJSON_Delete(root);
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
        app_mqtt_publish("chess/system/pong", response, strlen(response));
        LOG_INF("Responded to ping");
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
