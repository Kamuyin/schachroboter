#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>
#include <strings.h>
#include "mqtt_client.h"
#include "app_config.h"
#include "network_config.h"
#include "mdns_client.h"

LOG_MODULE_REGISTER(mqtt_client, LOG_LEVEL_INF);

#define MAX_SUBSCRIPTIONS 16

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

static int broker_from_static(struct sockaddr_in *out)
{
#ifdef MQTT_BROKER_STATIC_IP
    if (MQTT_BROKER_STATIC_IP[0] != '\0') {
        struct in_addr ip = {0};
        if (inet_pton(AF_INET, MQTT_BROKER_STATIC_IP, &ip) == 1) {
            memset(out, 0, sizeof(*out));
            out->sin_family = AF_INET;
            out->sin_addr = ip;
            out->sin_port = htons(MQTT_BROKER_PORT);
            LOG_WRN("Using statically configured MQTT broker %s:%d",
                    MQTT_BROKER_STATIC_IP, MQTT_BROKER_PORT);
            return 0;
        }
        LOG_WRN("MQTT_BROKER_STATIC_IP is invalid: %s", MQTT_BROKER_STATIC_IP);
    }
#endif
    return -ENOENT;
}

static int mqtt_broker_connect(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    int ret;

    LOG_INF("Discovering MQTT broker via mDNS (_mqtt._tcp.local)");

    struct sockaddr_in mdns_found;
    uint16_t mdns_port = 0;
    memset(&mdns_found, 0, sizeof(mdns_found));
    ret = mdns_browse_mqtt(&mdns_found, &mdns_port, 10000);
    if (ret == 0) {
        memcpy(broker4, &mdns_found, sizeof(struct sockaddr_in));
        if (mdns_port != 0) {
            broker4->sin_port = htons(mdns_port);
        }
    } else if (broker_from_static(broker4) == 0) {
        ret = 0;
    } else {
        LOG_ERR("Unable to resolve MQTT broker via mDNS and no static fallback configured (%d)", ret);
        return ret;
    }

    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &broker4->sin_addr, addr_str, sizeof(addr_str));
    LOG_INF("MQTT broker resolved at %s:%u", addr_str, ntohs(broker4->sin_port));

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

    LOG_INF("Connected to MQTT broker (TCP established, awaiting CONNACK)");
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
        /* Wait for network interface and carrier to be up before reconnecting */
        struct net_if *iface = network_get_interface();
        int carrier_waits = 0;
        while (!iface || !net_if_is_up(iface) || !net_if_is_carrier_ok(iface)) {
            if (carrier_waits == 0) {
                LOG_WRN("MQTT: Waiting for network interface and carrier to be up before reconnecting...");
            }
            carrier_waits++;
            k_sleep(K_SECONDS(2));
            iface = network_get_interface();
        }

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

        // Wait for MQTT CONNACK before proceeding
        {
            uint32_t deadline = k_uptime_get_32() + 5000; /* 5s */
            while (!mqtt_connected && (int32_t)(deadline - (int32_t)k_uptime_get_32()) > 0) {
                int wait = 100;
                int pr = poll(&fds, 1, wait);
                if (pr < 0) {
                    LOG_ERR("Poll error while waiting CONNACK: %d", errno);
                    break;
                }
                mqtt_input(&client_ctx);
            }
        }

        if (!mqtt_connected) {
            LOG_ERR("Timed out waiting for MQTT CONNACK");
            (void)mqtt_disconnect(&client_ctx, 0);
            continue;
        }

        LOG_INF("MQTT connection established (CONNACK received)");

        // Subscribe to all registered topics now that we're connected
        subscribe_to_topics();

        /* Publish online status to help verify connectivity */
        {
            char buf[96];
            int len = snprintk(buf, sizeof(buf), "{\"status\":\"online\",\"timestamp\":%u}", k_uptime_get_32());
            if (len > 0) {
                int pr = app_mqtt_publish("chess/system/online", buf, (uint32_t)len);
                if (pr < 0) {
                    LOG_WRN("Failed to publish online status: %d", pr);
                } else {
                    LOG_INF("Published online status");
                }
            }
        }

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

bool app_mqtt_is_connected(void)
{
    return mqtt_connected;
}
