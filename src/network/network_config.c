#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/logging/log.h>
#include "network_config.h"
#include "app_config.h"

LOG_MODULE_REGISTER(network_config, LOG_LEVEL_INF);

static struct net_if *iface = NULL;

struct net_if *network_get_interface(void)
{
    return iface;
}

int network_init(void)
{
    iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return -ENODEV;
    }

    LOG_INF("Network interface initialized");
    return 0;
}

int network_configure_static_ip(void)
{
    struct in_addr addr;
    struct in_addr netmask;
    struct in_addr gateway;

    if (!iface) {
        LOG_ERR("Network interface not initialized");
        return -EINVAL;
    }

    if (net_addr_pton(AF_INET, STATIC_IPV4_ADDR, &addr) < 0) {
        LOG_ERR("Invalid static IP address");
        return -EINVAL;
    }

    if (net_addr_pton(AF_INET, STATIC_IPV4_NETMASK, &netmask) < 0) {
        LOG_ERR("Invalid netmask");
        return -EINVAL;
    }

    if (net_addr_pton(AF_INET, STATIC_IPV4_GATEWAY, &gateway) < 0) {
        LOG_ERR("Invalid gateway");
        return -EINVAL;
    }

    net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
    net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask);
    net_if_ipv4_set_gw(iface, &gateway);

    LOG_INF("Static IP configured: %s", STATIC_IPV4_ADDR);
    return 0;
}
