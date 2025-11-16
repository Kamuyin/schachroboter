#ifndef MDNS_CLIENT_H
#define MDNS_CLIENT_H

#include <zephyr/net/socket.h>

int mdns_browse_mqtt(struct sockaddr_in *out_addr, uint16_t *out_port, int timeout_ms);

#endif /* MDNS_CLIENT_H */
