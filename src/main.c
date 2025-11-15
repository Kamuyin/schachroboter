#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_config.h"
#include "network_config.h"
#include "dhcp_server.h"
#include "mqtt_client.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(mqtt_client_stack, MQTT_THREAD_STACK_SIZE);

static struct k_thread mqtt_client_thread_data;

int main(void)
{
    int ret;

    LOG_INF("Starting Nucleo F767ZI Router Firmware");

    ret = network_init();
    if (ret < 0) {
        LOG_ERR("Network initialization failed: %d", ret);
        return ret;
    }

    ret = network_configure_static_ip();
    if (ret < 0) {
        LOG_ERR("Static IP configuration failed: %d", ret);
        return ret;
    }

    ret = dhcp_server_init();
    if (ret < 0) {
        LOG_ERR("DHCP server initialization failed: %d", ret);
        return ret;
    }

    ret = app_mqtt_init();
    if (ret < 0) {
        LOG_ERR("MQTT client initialization failed: %d", ret);
        return ret;
    }

    k_thread_create(&mqtt_client_thread_data, mqtt_client_stack,
                    K_THREAD_STACK_SIZEOF(mqtt_client_stack),
                    mqtt_client_thread,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("System initialized successfully");

    return 0;
}
