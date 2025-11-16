#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include "mqtt_client.h"
#include "app_config.h"

LOG_MODULE_REGISTER(mqtt_client, LOG_LEVEL_INF);

#define MAX_SUBSCRIPTIONS 4

static uint8_t rx_buffer[128];
static uint8_t tx_buffer[128];
static uint8_t payload_buffer[256];
static struct mqtt_client client_ctx;
static struct sockaddr_storage broker;
static struct pollfd fds;

typedef struct {
    char topic[64];
    mqtt_message_callback_t callback;
    bool active;
} mqtt_subscription_t;

static mqtt_subscription_t subscriptions[MAX_SUBSCRIPTIONS];
static bool mqtt_connected = false;

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result == 0) {
            LOG_INF("MQTT client connected");
            mqtt_connected = true;
        } else {
            LOG_ERR("MQTT connection failed: %d", evt->result);
        }
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_WRN("MQTT client disconnected: %d", evt->result);
        mqtt_connected = false;
        break;

    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *pub = &evt->param.publish;
        uint32_t payload_len = pub->message.payload.len;
        
        if (payload_len > sizeof(payload_buffer)) {
            payload_len = sizeof(payload_buffer);
        }

        int ret = mqtt_read_publish_payload(client, payload_buffer, payload_len);
        if (ret < 0) {
            LOG_ERR("Failed to read payload: %d", ret);
            break;
        }

        for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
            if (subscriptions[i].active && subscriptions[i].callback) {
                if (pub->message.topic.topic.size == strlen(subscriptions[i].topic) &&
                    memcmp(pub->message.topic.topic.utf8, subscriptions[i].topic, 
                           pub->message.topic.topic.size) == 0) {
                    subscriptions[i].callback(subscriptions[i].topic, payload_buffer, ret);
                }
            }
        }

        if (pub->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
            struct mqtt_puback_param ack = {
                .message_id = pub->message_id
            };
            mqtt_publish_qos1_ack(client, &ack);
        }
        break;
    }

    case MQTT_EVT_PUBACK:
        LOG_DBG("MQTT PUBACK received");
        break;

    case MQTT_EVT_SUBACK:
        LOG_INF("MQTT SUBACK received");
        break;

    default:
        break;
    }
}

static int mqtt_broker_connect(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    struct zsock_addrinfo hints = {0};
    struct zsock_addrinfo *res;
    int ret;

    LOG_INF("Resolving MQTT broker hostname: %s", MQTT_BROKER_HOSTNAME);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = zsock_getaddrinfo(MQTT_BROKER_HOSTNAME, NULL, &hints, &res);
    if (ret != 0) {
        LOG_ERR("DNS resolution failed for %s: %d", MQTT_BROKER_HOSTNAME, ret);
        return -ENOENT;
    }

    memcpy(broker4, res->ai_addr, sizeof(struct sockaddr_in));
    broker4->sin_port = htons(MQTT_BROKER_PORT);
    
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &broker4->sin_addr, addr_str, sizeof(addr_str));
    LOG_INF("Resolved %s to %s:%d", MQTT_BROKER_HOSTNAME, addr_str, MQTT_BROKER_PORT);

    zsock_freeaddrinfo(res);

    mqtt_client_init(&client_ctx);

    client_ctx.broker = &broker;
    client_ctx.evt_cb = mqtt_evt_handler;
    client_ctx.client_id.utf8 = (uint8_t *)MQTT_CLIENT_ID;
    client_ctx.client_id.size = strlen(MQTT_CLIENT_ID);
    client_ctx.password = NULL;
    client_ctx.user_name = NULL;
    client_ctx.protocol_version = MQTT_VERSION_3_1_1;
    client_ctx.rx_buf = rx_buffer;
    client_ctx.rx_buf_size = sizeof(rx_buffer);
    client_ctx.tx_buf = tx_buffer;
    client_ctx.tx_buf_size = sizeof(tx_buffer);
    client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

    ret = mqtt_connect(&client_ctx);
    if (ret < 0) {
        LOG_ERR("MQTT connect failed: %d", ret);
        return ret;
    }

    fds.fd = client_ctx.transport.tcp.sock;
    fds.events = POLLIN;

    LOG_INF("Connected to MQTT broker");
    return 0;
}

int app_mqtt_init(void)
{
    LOG_INF("MQTT client initialized");
    return 0;
}

static void subscribe_to_topics(void)
{
    int ret;

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subscriptions[i].active) {
            struct mqtt_topic sub_topic = {
                .topic.utf8 = (uint8_t *)subscriptions[i].topic,
                .topic.size = strlen(subscriptions[i].topic),
                .qos = MQTT_QOS_1_AT_LEAST_ONCE
            };

            struct mqtt_subscription_list sub_list = {
                .list = &sub_topic,
                .list_count = 1,
                .message_id = sys_rand32_get()
            };

            ret = mqtt_subscribe(&client_ctx, &sub_list);
            if (ret < 0) {
                LOG_ERR("Failed to subscribe to %s: %d", subscriptions[i].topic, ret);
            } else {
                LOG_INF("Subscribed to %s", subscriptions[i].topic);
            }
        }
    }
}

void mqtt_client_thread(void *p1, void *p2, void *p3)
{
    int ret;
    const uint32_t retry_delay_sec = 15;
    bool first_attempt = true;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("MQTT client thread started");

    while (1) {
        if (first_attempt) {
            k_sleep(K_SECONDS(5));
            first_attempt = false;
        } else {
            LOG_INF("Retrying connection in %u seconds...", retry_delay_sec);
            k_sleep(K_SECONDS(retry_delay_sec));
        }

        ret = mqtt_broker_connect();
        if (ret < 0) {
            LOG_ERR("Failed to connect to broker, will retry");
            continue;
        }

        // Connection successful
        LOG_INF("MQTT connection established");

        k_sleep(K_MSEC(500));

        // Subscribe to all registered topics
        subscribe_to_topics();

        // Main MQTT processing loop
        while (mqtt_connected) {
            ret = poll(&fds, 1, mqtt_keepalive_time_left(&client_ctx));
            if (ret < 0) {
                LOG_ERR("Poll error: %d", errno);
                break;
            }

            mqtt_input(&client_ctx);

            if ((fds.revents & POLLERR) || (fds.revents & POLLNVAL)) {
                LOG_ERR("Socket error");
                break;
            }

            ret = mqtt_live(&client_ctx);
            if (ret < 0 && ret != -EAGAIN) {
                LOG_ERR("MQTT live error: %d", ret);
                break;
            }

            k_sleep(K_MSEC(100));
        }

        LOG_WRN("MQTT connection lost, attempting to reconnect");
        (void)mqtt_disconnect(&client_ctx, 0);
        mqtt_connected = false;
    }
}

int app_mqtt_publish(const char *topic, const char *payload, uint32_t payload_len)
{
    struct mqtt_publish_param param;

    if (!mqtt_connected) {
        return -ENOTCONN;
    }

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)payload;
    param.message.payload.len = payload_len;
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    return mqtt_publish(&client_ctx, &param);
}

int app_mqtt_subscribe(const char *topic, mqtt_message_callback_t callback)
{
    int slot = -1;

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (!subscriptions[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        LOG_ERR("No subscription slots available");
        return -ENOMEM;
    }

    strncpy(subscriptions[slot].topic, topic, sizeof(subscriptions[slot].topic) - 1);
    subscriptions[slot].callback = callback;
    subscriptions[slot].active = true;

    if (mqtt_connected) {
        struct mqtt_topic sub_topic = {
            .topic.utf8 = (uint8_t *)subscriptions[slot].topic,
            .topic.size = strlen(subscriptions[slot].topic),
            .qos = MQTT_QOS_1_AT_LEAST_ONCE
        };

        struct mqtt_subscription_list sub_list = {
            .list = &sub_topic,
            .list_count = 1,
            .message_id = sys_rand32_get()
        };

        int ret = mqtt_subscribe(&client_ctx, &sub_list);
        if (ret < 0) {
            LOG_ERR("Failed to subscribe to %s: %d", topic, ret);
            subscriptions[slot].active = false;
            return ret;
        }

        LOG_INF("Subscribed to %s", topic);
    }

    return 0;
}
