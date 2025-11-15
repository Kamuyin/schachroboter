#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <zephyr/net/mqtt.h>

int app_mqtt_init(void);
void mqtt_client_thread(void *p1, void *p2, void *p3);
int app_mqtt_publish(const char *topic, const char *payload, uint32_t payload_len);

#endif