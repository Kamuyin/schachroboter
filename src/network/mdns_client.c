#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <poll.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/logging/log.h>

#include "mdns_client.h"

LOG_MODULE_DECLARE(mqtt_client);

#define MDNS_GROUP_ADDR 0xE00000FB /* 224.0.0.251 */
#define MDNS_PORT       5353

struct __packed dns_hdr {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

static size_t mdns_encode_qname(uint8_t *buf, size_t buflen, const char *name)
{
    size_t nlen = strlen(name);
    if (nlen + 2 > buflen) {
        return 0;
    }

    size_t bi = 0;
    size_t li = 0;
    for (size_t i = 0; i <= nlen; i++) {
        if (name[i] == '.' || name[i] == '\0') {
            size_t lab_len = i - li;
            if (lab_len > 63 || bi + 1 + lab_len + 1 > buflen) {
                return 0;
            }
            buf[bi++] = (uint8_t)lab_len;
            memcpy(&buf[bi], &name[li], lab_len);
            bi += lab_len;
            li = i + 1;
        }
    }
    buf[bi++] = 0;
    return bi;
}

static int mdns_decode_name(const uint8_t *msg, size_t msglen, size_t *off_io,
                            char *out, size_t outlen)
{
    size_t off = *off_io;
    size_t outi = 0;
    bool jumped = false;
    size_t jump_end = 0;

    while (off < msglen) {
        uint8_t len = msg[off++];
        if (len == 0) {
            *off_io = jumped ? jump_end : off;
            if (outi < outlen && out) {
                out[outi] = '\0';
            }
            return 0;
        }
        if ((len & 0xC0) == 0xC0) {
            if (off >= msglen) {
                return -EINVAL;
            }
            uint16_t ptr = ((len & 0x3F) << 8) | msg[off++];
            if (ptr >= msglen) {
                return -EINVAL;
            }
            if (!jumped) {
                jump_end = off;
                jumped = true;
            }
            off = ptr;
            continue;
        }
        if (len > 63 || off + len > msglen) {
            return -EINVAL;
        }
        if (outi && out && outi < outlen) {
            out[outi++] = '.';
        }
        size_t avail = (out && outlen > outi) ? (outlen - 1 - outi) : 0;
        size_t copy = (len < avail) ? len : avail;
        if (copy && out) {
            memcpy(&out[outi], &msg[off], copy);
            outi += copy;
        }
        off += len;
    }
    return -EINVAL;
}

static int mdns_send_ptr_query(int sock)
{
    uint8_t buf[512];
    struct dns_hdr *hdr = (struct dns_hdr *)buf;
    memset(buf, 0, sizeof(buf));
    hdr->qdcount = htons(1);

    size_t off = sizeof(struct dns_hdr);
    const char *svc = "_mqtt._tcp.local";
    size_t nsz = mdns_encode_qname(&buf[off], sizeof(buf) - off, svc);
    if (!nsz) {
        return -EINVAL;
    }
    off += nsz;
    if (off + 4 > sizeof(buf)) {
        return -EINVAL;
    }
    buf[off++] = 0;
    buf[off++] = 12; /* PTR */
    uint16_t qclass = htons(0x8000 | 1);
    memcpy(&buf[off], &qclass, 2);
    off += 2;

    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port = htons(MDNS_PORT),
        .sin_addr.s_addr = htonl(MDNS_GROUP_ADDR)
    };

    return sendto(sock, buf, off, 0, (struct sockaddr *)&dst, sizeof(dst));
}

static int mdns_send_query(int sock, const char *name, uint16_t qtype)
{
    uint8_t buf[512];
    struct dns_hdr *hdr = (struct dns_hdr *)buf;
    memset(buf, 0, sizeof(buf));
    hdr->qdcount = htons(1);

    size_t off = sizeof(struct dns_hdr);
    size_t nsz = mdns_encode_qname(&buf[off], sizeof(buf) - off, name);
    if (!nsz) {
        return -EINVAL;
    }
    off += nsz;
    if (off + 4 > sizeof(buf)) {
        return -EINVAL;
    }
    buf[off++] = (uint8_t)((qtype >> 8) & 0xFF);
    buf[off++] = (uint8_t)(qtype & 0xFF);
    uint16_t qclass = htons(0x8000 | 1);
    memcpy(&buf[off], &qclass, 2);
    off += 2;

    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port = htons(MDNS_PORT),
        .sin_addr.s_addr = htonl(MDNS_GROUP_ADDR)
    };

    return sendto(sock, buf, off, 0, (struct sockaddr *)&dst, sizeof(dst));
}

int mdns_browse_mqtt(struct sockaddr_in *out_addr, uint16_t *out_port, int timeout_ms)
{
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        int err = -errno;
        LOG_INF("mDNS: socket() failed: %d", err);
        return err;
    }

    int ttl = 255;
    (void)setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    (void)setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    struct ip_mreq mreq = {
        .imr_multiaddr.s_addr = htonl(MDNS_GROUP_ADDR),
        .imr_interface.s_addr = htonl(INADDR_ANY)
    };
    (void)setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    struct in_addr ifaddr = { .s_addr = htonl(INADDR_ANY) };
    (void)setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr, sizeof(ifaddr));

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(0),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    if (bind(s, (struct sockaddr *)&local, sizeof(local)) < 0) {
        int err = -errno;
        LOG_INF("mDNS: bind() failed: %d", err);
        close(s);
        return err;
    }

    int ret = mdns_send_ptr_query(s);
    if (ret < 0) {
        LOG_INF("mDNS: send query failed: %d", ret);
        close(s);
        return ret;
    }
    LOG_INF("mDNS: PTR query for _mqtt._tcp.local sent (bytes=%d)", ret);

    char target_host[128] = {0};
    uint16_t target_port = 0;
    char a_for[128] = {0};
    struct in_addr a_addr = {0};
    char instance_name[160] = {0};
    struct pollfd p = { .fd = s, .events = POLLIN };
    int deadline = k_uptime_get_32() + timeout_ms;

    while ((int32_t)(deadline - (int32_t)k_uptime_get_32()) > 0) {
        int wait = deadline - (int32_t)k_uptime_get_32();
        if (wait < 0) {
            wait = 0;
        }
        ret = poll(&p, 1, wait);
        if (ret <= 0) {
            break;
        }
        if (p.revents & POLLIN) {
            uint8_t buf[768];
            int len = recv(s, buf, sizeof(buf), 0);
            if (len <= (int)sizeof(struct dns_hdr)) {
                continue;
            }

            struct dns_hdr *hdr = (struct dns_hdr *)buf;
            size_t off = sizeof(struct dns_hdr);
            uint16_t qd = ntohs(hdr->qdcount);
            uint16_t an = ntohs(hdr->ancount);
            uint16_t ns = ntohs(hdr->nscount);
            uint16_t ar = ntohs(hdr->arcount);
            LOG_INF("mDNS: received message qd=%u an=%u ns=%u ar=%u", qd, an, ns, ar);

            for (uint16_t i = 0; i < qd; i++) {
                if (mdns_decode_name(buf, len, &off, NULL, 0) < 0) {
                    off = len;
                    break;
                }
                if (off + 4 > (size_t)len) {
                    off = len;
                    break;
                }
                off += 4;
            }

            uint16_t total = an + ns + ar;
            for (uint16_t i = 0; i < total && off + 10 <= (size_t)len; i++) {
                char rrname[128];
                if (mdns_decode_name(buf, len, &off, rrname, sizeof(rrname)) < 0) {
                    break;
                }
                if (off + 10 > (size_t)len) {
                    break;
                }
                uint16_t type = (buf[off] << 8) | buf[off + 1];
                uint16_t rdlen = (buf[off + 8] << 8) | buf[off + 9];
                off += 10;
                if (off + rdlen > (size_t)len) {
                    break;
                }
                LOG_DBG("mDNS: RR name=%s type=%u rdlen=%u", rrname, type, rdlen);

                if (type == 12) {
                    size_t off2 = off;
                    char inst[160];
                    if (mdns_decode_name(buf, len, &off2, inst, sizeof(inst)) == 0) {
                        if (strncasecmp(rrname, "_mqtt._tcp.local", sizeof(rrname)) == 0) {
                            if (instance_name[0] == '\0') {
                                strncpy(instance_name, inst, sizeof(instance_name) - 1);
                                LOG_INF("mDNS: PTR instance %s", instance_name);
                            }
                        }
                    }
                }
                if (type == 33 && rdlen >= 6) {
                    uint16_t port = (buf[off + 4] << 8) | buf[off + 5];
                    size_t off2 = off + 6;
                    char target[128];
                    if (mdns_decode_name(buf, len, &off2, target, sizeof(target)) == 0) {
                        strncpy(target_host, target, sizeof(target_host) - 1);
                        target_port = port;
                        LOG_INF("mDNS: SRV %s port %u", target_host, target_port);
                    }
                } else if (type == 1 && rdlen == 4) {
                    if (target_host[0] == '\0' || strcmp(rrname, target_host) == 0) {
                        memcpy(&a_addr, &buf[off], 4);
                        strncpy(a_for, rrname, sizeof(a_for) - 1);
                        char ipbuf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &a_addr, ipbuf, sizeof(ipbuf));
                        LOG_INF("mDNS: A %s -> %s", a_for, ipbuf);
                    }
                }
                off += rdlen;
            }

            if (target_host[0] && a_for[0] && strcmp(target_host, a_for) == 0 &&
                a_addr.s_addr != 0 && target_port != 0) {
                out_addr->sin_family = AF_INET;
                out_addr->sin_addr = a_addr;
                out_addr->sin_port = htons(target_port);
                *out_port = target_port;
                close(s);
                return 0;
            }

            if (instance_name[0] && target_host[0] == 0) {
                int rs = mdns_send_query(s, instance_name, 33);
                if (rs >= 0) {
                    LOG_INF("mDNS: sent SRV query for %s (bytes=%d)", instance_name, rs);
                }
            }
            if (target_host[0] && a_addr.s_addr == 0) {
                int ra = mdns_send_query(s, target_host, 1);
                if (ra >= 0) {
                    LOG_INF("mDNS: sent A query for %s (bytes=%d)", target_host, ra);
                }
            }
        }
    }

    close(s);
    return -ETIMEDOUT;
}
