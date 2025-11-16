#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define STATIC_IPV4_ADDR "192.168.1.1"
#define STATIC_IPV4_NETMASK "255.255.255.0"
#define STATIC_IPV4_GATEWAY "192.168.1.1"

#define DHCP_SERVER_POOL_START "192.168.1.100"
#define DHCP_SERVER_POOL_END "192.168.1.200"

#define MQTT_BROKER_HOSTNAME "mqtt-broker.local"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "controller"

/* Optional: static fallback if mDNS/DNS-SD discovery fails
	Uncomment and set to the broker IPv4 address to force a last-resort connect.
	Example: "192.168.1.100" */
// #define MQTT_BROKER_STATIC_IP ""

#define DHCP_THREAD_STACK_SIZE 2048
#define MQTT_THREAD_STACK_SIZE 4096
#define THREAD_PRIORITY 5

#endif