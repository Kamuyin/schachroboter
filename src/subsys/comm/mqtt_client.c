#include "subsys/comm/mqtt_client.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <string.h>
LOG_MODULE_REGISTER(mqttc, LOG_LEVEL_INF);

static mqttc_cfg_t g_cfg;
static struct mqtt_client client;
static struct sockaddr_storage broker;
static mqtt_cmd_cb_t g_on_cmd;
static uint8_t rx_buffer[1024];
static uint8_t tx_buffer[1024];
// Remove unused pollfd array for now

static void prepare_broker(void) {
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(g_cfg.broker_port);
    zsock_inet_pton(AF_INET, g_cfg.broker_host, &broker4->sin_addr);
}

static void mqtt_evt(struct mqtt_client *const c, const struct mqtt_evt *evt) {
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        LOG_INF("MQTT connected: %d", evt->result);
        mqtt_subscribe(c, &(struct mqtt_subscription_list){
            .list = (struct mqtt_topic[]){
                {.topic = {.utf8 = (uint8_t*)proto_topic_cmd(), .size = strlen(proto_topic_cmd())}, .qos = MQTT_QOS_0_AT_MOST_ONCE}
            }, .list_count=1, .message_id=1});
        break;
    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *p = &evt->param.publish;
        char buf[512];
        size_t n = MIN(p->message.payload.len, sizeof(buf)-1);
        memcpy(buf, p->message.payload.data, n);
        buf[n]=0;
        cmd_t cmd;
        if (proto_decode_cmd(buf, &cmd) && g_on_cmd) g_on_cmd(&cmd);
        mqtt_publish_qos1_ack(c, p->message_id);
        break;
    }
    default:
        break;
    }
}

int mqttc_init(const mqttc_cfg_t *cfg, mqtt_cmd_cb_t on_cmd) {
    g_cfg = *cfg; g_on_cmd = on_cmd;
    prepare_broker();

    mqtt_client_init(&client);
    client.broker = &broker;
    client.evt_cb = mqtt_evt;
    client.client_id.utf8 = (uint8_t *)g_cfg.client_id;
    client.client_id.size = strlen(g_cfg.client_id);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.transport.type = MQTT_TRANSPORT_NON_SECURE;

    client.rx_buf = rx_buffer; client.rx_buf_size = sizeof(rx_buffer);
    client.tx_buf = tx_buffer; client.tx_buf_size = sizeof(tx_buffer);
    return 0;
}

int mqttc_start(void) {
    int rc = mqtt_connect(&client);
    if (rc) return rc;
    // Simplified background pump - remove polling for now
    while (1) {
        mqtt_input(&client);
        mqtt_live(&client);
        k_msleep(1000);
    }
    return 0;
}

int mqttc_publish_status(const uint8_t board[8], const pos_t *pos, int last_err) {
    char payload[384];
    int n = proto_encode_status(payload, sizeof(payload), board, pos, last_err);
    if (n <= 0) return -EINVAL;

    struct mqtt_publish_param p = {0};
    p.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
    p.message.topic.topic.utf8 = (uint8_t *)proto_topic_status();
    p.message.topic.topic.size = strlen(proto_topic_status());
    p.message.payload.data = payload;
    p.message.payload.len = strlen(payload);
    p.message_id = 1;
    return mqtt_publish(&client, &p);
}