#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include "dhcp_server.h"
#include "network_config.h"
#include "app_config.h"

LOG_MODULE_REGISTER(dhcp_server, LOG_LEVEL_DBG);

int dhcp_server_init(void)
{
    struct net_if *iface;
    struct in_addr base_addr;
    struct in_addr end_addr;
    int ret;

    iface = network_get_interface();
    if (!iface) {
        LOG_ERR("Network interface not available");
        return -ENODEV;
    }

    LOG_INF("Initializing DHCP server");
    LOG_INF("Pool start: %s", DHCP_SERVER_POOL_START);
    LOG_INF("Pool end: %s", DHCP_SERVER_POOL_END);
    LOG_INF("Interface is up: %s", net_if_is_up(iface) ? "yes" : "no");

    if (net_addr_pton(AF_INET, DHCP_SERVER_POOL_START, &base_addr) < 0) {
        LOG_ERR("Invalid DHCP pool base address");
        return -EINVAL;
    }

    if (net_addr_pton(AF_INET, DHCP_SERVER_POOL_END, &end_addr) < 0) {
        LOG_ERR("Invalid DHCP pool end address");
        return -EINVAL;
    }

    LOG_INF("Starting DHCP server with base address: %s", DHCP_SERVER_POOL_START);
    
    ret = net_dhcpv4_server_start(iface, &base_addr);
    if (ret < 0) {
        LOG_ERR("Failed to start DHCP server: %d", ret);
        LOG_ERR("Error code: %s", strerror(-ret));
        return ret;
    }

    LOG_INF("DHCP server started successfully");
    LOG_INF("Listening for DHCP requests on interface %d", net_if_get_by_iface(iface));
    
    return 0;
}
