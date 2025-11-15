#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <zephyr/net/net_if.h>

int network_init(void);
int network_configure_static_ip(void);
struct net_if *network_get_interface(void);

#endif