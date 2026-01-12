#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_config.h"
#include "network_config.h"
#include "dhcp_server.h"
#include "mqtt_client.h"
#include "application.h"
#include "robot_controller.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(mqtt_client_stack, MQTT_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(application_stack, 4096);
K_THREAD_STACK_DEFINE(robot_controller_stack, 2048);

static struct k_thread mqtt_client_thread_data;
static struct k_thread application_thread_data;
static struct k_thread robot_controller_thread_data;

int main(void)
{
    int ret;

    LOG_DBG("Network initializing...");
    ret = network_init();
    if (ret < 0) {
        LOG_ERR("Network initialization failed: %d", ret);
        return ret;
    }

    k_sleep(K_MSEC(500));


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

    ret = robot_controller_init();
    if (ret < 0) {
        LOG_ERR("Robot controller initialization failed: %d", ret);
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

    k_thread_create(&robot_controller_thread_data, robot_controller_stack,
                    K_THREAD_STACK_SIZEOF(robot_controller_stack),
                    (k_thread_entry_t)robot_controller_task,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    LOG_INF("System is ready");

    return 0;
}
