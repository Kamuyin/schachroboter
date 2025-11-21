#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <zephyr/net/mqtt.h>

typedef void (*mqtt_message_callback_t)(const char *topic, const uint8_t *payload, uint32_t payload_len);

int app_mqtt_init(void);
void mqtt_client_thread(void *p1, void *p2, void *p3);
int app_mqtt_publish(const char *topic, const char *payload, uint32_t payload_len);
int app_mqtt_subscribe(const char *topic, mqtt_message_callback_t callback);

/* Returns true if the MQTT client has an active connection (CONNACK received) */
bool app_mqtt_is_connected(void);

#endif