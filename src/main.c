#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_config.h"
#include "network_config.h"
#include "dhcp_server.h"
#include "mqtt_client.h"
#include "application.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(mqtt_client_stack, MQTT_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(application_stack, 4096);

static struct k_thread mqtt_client_thread_data;
static struct k_thread application_thread_data;

int main(void)
{
    int ret;

    LOG_INF("Starting Nucleo F767ZI Router Firmware");

    LOG_INF("Step 1: Initializing network interface");
    ret = network_init();
    if (ret < 0) {
        LOG_ERR("Network initialization failed: %d", ret);
        return ret;
    }

    // Give the interface time to come up
    LOG_INF("Waiting for interface to be ready...");
    k_sleep(K_MSEC(500));

    LOG_INF("Step 2: Configuring static IP address");
    ret = network_configure_static_ip();
    if (ret < 0) {
        LOG_ERR("Static IP configuration failed: %d", ret);
        return ret;
    }

    // Give IP configuration time to settle
    LOG_INF("Waiting for IP configuration to settle...");
    k_sleep(K_MSEC(500));

    LOG_INF("Step 3: Starting DHCP server");
    ret = dhcp_server_init();
    if (ret < 0) {
        LOG_ERR("DHCP server initialization failed: %d", ret);
        return ret;
    }

    LOG_INF("Step 4: Initializing MQTT client");
    ret = app_mqtt_init();
    if (ret < 0) {
        LOG_ERR("MQTT client initialization failed: %d", ret);
        return ret;
    }

    ret = application_init();
    if (ret < 0) {
        LOG_ERR("Application initialization failed: %d", ret);
        return ret;
    }

    k_thread_create(&mqtt_client_thread_data, mqtt_client_stack,
                    K_THREAD_STACK_SIZEOF(mqtt_client_stack),
                    mqtt_client_thread,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&application_thread_data, application_stack,
                    K_THREAD_STACK_SIZEOF(application_stack),
                    (k_thread_entry_t)application_task,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("System initialized successfully");
    LOG_INF("DHCP server ready to serve clients");

    return 0;
}
