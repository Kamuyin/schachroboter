#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/logging/log.h>
#include "dhcp_server.h"
#include "network_config.h"
#include "app_config.h"

LOG_MODULE_REGISTER(dhcp_server, LOG_LEVEL_INF);

int dhcp_server_init(void)
{
    struct net_if *iface;
    struct in_addr base_addr;
    int ret;

    iface = network_get_interface();
    if (!iface) {
        LOG_ERR("Network interface not available");
        return -ENODEV;
    }

    if (net_addr_pton(AF_INET, DHCP_SERVER_POOL_START, &base_addr) < 0) {
        LOG_ERR("Invalid DHCP pool base address");
        return -EINVAL;
    }

    ret = net_dhcpv4_server_start(iface, &base_addr);
    if (ret < 0) {
        LOG_ERR("Failed to start DHCP server: %d", ret);
        return ret;
    }

    LOG_INF("DHCP server started");
    return 0;
}
