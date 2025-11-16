#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/logging/log.h>
#include "network_config.h"
#include "app_config.h"

LOG_MODULE_REGISTER(network_config, LOG_LEVEL_INF);

static struct net_if *iface = NULL;
static bool carrier_was_ok = false;

#define CARRIER_MONITOR_STACK_SIZE 1024
static K_THREAD_STACK_DEFINE(carrier_monitor_stack, CARRIER_MONITOR_STACK_SIZE);
static struct k_thread carrier_monitor_thread;

static void carrier_monitor_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        k_msleep(1000); // Check every second
        
        if (!iface) {
            continue;
        }
        
        bool carrier_ok = net_if_is_carrier_ok(iface);
        
        if (carrier_ok && !carrier_was_ok) {
            LOG_WRN("*** CARRIER UP - Link restored ***");
            carrier_was_ok = true;
        } else if (!carrier_ok && carrier_was_ok) {
            LOG_ERR("*** CARRIER DOWN - Link lost! ***");
            LOG_ERR("*** DHCP server may not be reachable ***");
            carrier_was_ok = false;
        }
    }
}

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
    LOG_INF("Interface index: %d", net_if_get_by_iface(iface));
    LOG_INF("Interface up before bringing up: %s", net_if_is_up(iface) ? "yes" : "no");
    
    // Bring the interface up
    net_if_up(iface);
    LOG_INF("Interface brought up");
    
    // Wait for carrier/link detection (PHY link up)
    LOG_INF("Waiting for carrier detection...");
    int retries = 50; // 5 seconds max
    while (retries-- > 0) {
        if (net_if_is_carrier_ok(iface)) {
            LOG_INF("Carrier detected!");
            break;
        }
        k_msleep(100);
    }
    
    if (!net_if_is_carrier_ok(iface)) {
        LOG_WRN("No carrier detected - is cable plugged in?");
    }
    
    LOG_INF("Interface up after bringing up: %s", net_if_is_up(iface) ? "yes" : "no");
    LOG_INF("Carrier present: %s", net_if_is_carrier_ok(iface) ? "yes" : "no");
    
    // Initialize carrier monitoring state
    carrier_was_ok = net_if_is_carrier_ok(iface);
    
    // Start carrier monitoring thread
    k_thread_create(&carrier_monitor_thread, carrier_monitor_stack,
                    K_THREAD_STACK_SIZEOF(carrier_monitor_stack),
                    carrier_monitor_fn, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);
    k_thread_name_set(&carrier_monitor_thread, "carrier_mon");
    LOG_INF("Carrier monitoring thread started");
    
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

    LOG_INF("Configuring static IP: %s", STATIC_IPV4_ADDR);
    LOG_INF("Netmask: %s", STATIC_IPV4_NETMASK);
    LOG_INF("Gateway: %s", STATIC_IPV4_GATEWAY);

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
