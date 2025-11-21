#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>
#include <strings.h>
#include "mqtt_client.h"
#include "app_config.h"
#include "network_config.h"

LOG_MODULE_REGISTER(mqtt_client, LOG_LEVEL_DBG);

#define MAX_SUBSCRIPTIONS 4

static uint8_t rx_buffer[128];
static uint8_t tx_buffer[128];
static uint8_t payload_buffer[256];
static struct mqtt_client client_ctx;
static struct sockaddr_storage broker;
static struct pollfd fds;

typedef struct {
    char topic[64];
    mqtt_message_callback_t callback;
    bool active;
} mqtt_subscription_t;

static mqtt_subscription_t subscriptions[MAX_SUBSCRIPTIONS];
static bool mqtt_connected = false;

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result == 0) {
            LOG_INF("MQTT client connected");
            mqtt_connected = true;
        } else {
            LOG_ERR("MQTT connection failed: %d", evt->result);
        }
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_WRN("MQTT client disconnected: %d", evt->result);
        mqtt_connected = false;
        break;

    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *pub = &evt->param.publish;
        uint32_t payload_len = pub->message.payload.len;
        
        if (payload_len > sizeof(payload_buffer)) {
            payload_len = sizeof(payload_buffer);
        }

        int ret = mqtt_read_publish_payload(client, payload_buffer, payload_len);
        if (ret < 0) {
            LOG_ERR("Failed to read payload: %d", ret);
            break;
        }

        for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
            if (subscriptions[i].active && subscriptions[i].callback) {
                if (pub->message.topic.topic.size == strlen(subscriptions[i].topic) &&
                    memcmp(pub->message.topic.topic.utf8, subscriptions[i].topic, 
                           pub->message.topic.topic.size) == 0) {
                    subscriptions[i].callback(subscriptions[i].topic, payload_buffer, ret);
                }
            }
        }

        if (pub->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
            struct mqtt_puback_param ack = {
                .message_id = pub->message_id
            };
            mqtt_publish_qos1_ack(client, &ack);
        }
        break;
    }

    case MQTT_EVT_PUBACK:
        LOG_DBG("MQTT PUBACK received");
        break;

    case MQTT_EVT_SUBACK:
        LOG_INF("MQTT SUBACK received");
        break;

    default:
        break;
    }
}

// DNS-SD callback for service discovery
static K_SEM_DEFINE(dns_service_sem, 0, 1);
static K_SEM_DEFINE(dns_addr_sem, 0, 1);
static struct sockaddr_in resolved_addr;
static int dns_result = -1;
static uint16_t resolved_port = 0;
static char service_instance[128];
static struct dns_resolve_context dns_ctx;
static struct sockaddr_in mdns_sa_global;
static const struct sockaddr *mdns_servers_sa[2];
static K_SEM_DEFINE(dns_svc_sem, 0, 1);
static char srv_target_host[128];

static void dns_addr_result_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data)
{
    if (status == DNS_EAI_INPROGRESS && info) {
        if (info->ai_family == AF_INET) {
            memcpy(&resolved_addr, &info->ai_addr, sizeof(struct sockaddr_in));
            dns_result = 0;

            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &resolved_addr.sin_addr, addr_str, sizeof(addr_str));
            LOG_INF("A record resolved: %s", addr_str);
        }
        return;
    }

    if (status == DNS_EAI_ALLDONE) {
        k_sem_give(&dns_addr_sem);
        return;
    }

    if (status < 0) {
        LOG_ERR("A record resolution error: %d", status);
        dns_result = status;
        k_sem_give(&dns_addr_sem);
    }
}

static void dns_sd_result_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data)
{
    if (status == DNS_EAI_INPROGRESS && info) {
        // For service browse, Zephyr returns AF_LOCAL with instance FQDN in ai_canonname
        if (info->ai_family == AF_LOCAL) {
            size_t len = strnlen(info->ai_canonname, sizeof(service_instance) - 1);
            memset(service_instance, 0, sizeof(service_instance));
            memcpy(service_instance, info->ai_canonname, len);
            LOG_INF("Found service instance: %s", service_instance);
        }
        return;
    }

    if (status == DNS_EAI_ALLDONE) {
        LOG_INF("DNS-SD browse complete");
        k_sem_give(&dns_service_sem);
        return;
    }

    if (status < 0) {
        LOG_ERR("DNS-SD browse error: %d", status);
        dns_result = status;
        k_sem_give(&dns_service_sem);
    }
}

static void dns_service_resolve_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data)
{
    if (status == DNS_EAI_INPROGRESS && info) {
        if (info->ai_family == AF_INET) {
            memcpy(&resolved_addr, &info->ai_addr, sizeof(struct sockaddr_in));
            /* Capture port if provided by SRV resolution */
            resolved_port = ntohs(((struct sockaddr_in *)&info->ai_addr)->sin_port);
            dns_result = 0;
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &resolved_addr.sin_addr, addr_str, sizeof(addr_str));
            LOG_INF("Service resolved address: %s (port %u)", addr_str, resolved_port);
        } else if (info->ai_family == AF_LOCAL) {
            size_t len = strnlen(info->ai_canonname, sizeof(srv_target_host) - 1);
            memset(srv_target_host, 0, sizeof(srv_target_host));
            memcpy(srv_target_host, info->ai_canonname, len);
            LOG_DBG("Service resolve info (name): %s", srv_target_host);
        }
        return;
    }

    if (status == DNS_EAI_ALLDONE) {
        k_sem_give(&dns_svc_sem);
        return;
    }

    if (status < 0) {
        LOG_ERR("Service resolution error: %d", status);
        dns_result = status;
        k_sem_give(&dns_svc_sem);
    }
}

/* Minimal IPv4 mDNS/DNS-SD client for _mqtt._tcp.local */
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
    if (nlen + 2 > buflen) return 0;
    size_t bi = 0, li = 0;
    for (size_t i = 0; i <= nlen; i++) {
        if (name[i] == '.' || name[i] == '\0') {
            size_t lab_len = i - li;
            if (lab_len > 63 || bi + 1 + lab_len + 1 > buflen) return 0;
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
    int jumped = 0;
    size_t jump_end = 0;

    while (off < msglen) {
        uint8_t len = msg[off++];
        if (len == 0) {
            if (!jumped) {
                *off_io = off;
            } else {
                /* When jumped via compression, advance caller offset
                 * to the first byte after the compression pointer */
                *off_io = jump_end;
            }
            if (outi < outlen) out[outi] = '\0';
            return 0;
        }
        if ((len & 0xC0) == 0xC0) {
            if (off >= msglen) return -EINVAL;
            uint16_t ptr = ((len & 0x3F) << 8) | msg[off++];
            if (ptr >= msglen) return -EINVAL;
            if (!jumped) { jump_end = off; jumped = 1; }
            off = ptr;
            continue;
        }
        if (len > 63 || off + len > msglen) return -EINVAL;
        if (outi && outi < outlen) out[outi++] = '.';
        size_t avail = (outlen > outi) ? (outlen - 1 - outi) : 0;
        size_t copy = (len < avail) ? len : avail;
        if (copy) {
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
    hdr->id = 0;
    hdr->qdcount = htons(1);

    size_t off = sizeof(struct dns_hdr);
    const char *svc = "_mqtt._tcp.local";
    size_t nsz = mdns_encode_qname(&buf[off], sizeof(buf) - off, svc);
    if (!nsz) return -EINVAL;
    off += nsz;
    if (off + 4 > sizeof(buf)) return -EINVAL;
    buf[off++] = 0; buf[off++] = 12; /* QTYPE PTR */
    uint16_t qclass = htons(0x8000 | 1); /* IN with QU bit */
    memcpy(&buf[off], &qclass, 2); off += 2;

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
    hdr->id = 0;
    hdr->qdcount = htons(1);

    size_t off = sizeof(struct dns_hdr);
    size_t nsz = mdns_encode_qname(&buf[off], sizeof(buf) - off, name);
    if (!nsz) return -EINVAL;
    off += nsz;
    if (off + 4 > sizeof(buf)) return -EINVAL;
    buf[off++] = (uint8_t)((qtype >> 8) & 0xFF);
    buf[off++] = (uint8_t)(qtype & 0xFF);
    uint16_t qclass = htons(0x8000 | 1); /* IN with QU bit */
    memcpy(&buf[off], &qclass, 2); off += 2;

    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port = htons(MDNS_PORT),
        .sin_addr.s_addr = htonl(MDNS_GROUP_ADDR)
    };
    return sendto(sock, buf, off, 0, (struct sockaddr *)&dst, sizeof(dst));
}

static int mdns_browse_mqtt(struct sockaddr_in *out_addr, uint16_t *out_port, int timeout_ms)
{
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        int err = -errno;
        LOG_INF("mDNS: socket() failed: %d", err);
        return err;
    }

    /* RFC 6762: mDNS packets sent over multicast MUST have TTL=255 */
    {
        int ttl = 255;
        (void)setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
        (void)setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    }
    /* Try to join the mDNS multicast group to ensure reception of multicasts */
    {
        struct ip_mreq mreq = {
            .imr_multiaddr.s_addr = htonl(MDNS_GROUP_ADDR),
            .imr_interface.s_addr = htonl(INADDR_ANY)
        };
        (void)setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    }
    /* Prefer default interface for multicast; leave as INADDR_ANY if unknown */
    {
        struct in_addr ifaddr = { .s_addr = htonl(INADDR_ANY) };
        (void)setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr, sizeof(ifaddr));
    }
    /* Bind to ephemeral port to avoid conflict with Zephyr mDNS responder on 5353 */
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
        if (wait < 0) wait = 0;
        ret = poll(&p, 1, wait);
        if (ret <= 0) break;
        if (p.revents & POLLIN) {
            uint8_t buf[768];
            int len = recv(s, buf, sizeof(buf), 0);
            if (len <= (int)sizeof(struct dns_hdr)) continue;
            struct dns_hdr *hdr = (struct dns_hdr *)buf;
            size_t off = sizeof(struct dns_hdr);
            uint16_t qd = ntohs(hdr->qdcount);
            uint16_t an = ntohs(hdr->ancount);
            uint16_t ns = ntohs(hdr->nscount);
            uint16_t ar = ntohs(hdr->arcount);
            LOG_INF("mDNS: received message qd=%u an=%u ns=%u ar=%u", qd, an, ns, ar);
            for (uint16_t i = 0; i < qd; i++) {
                if (mdns_decode_name(buf, len, &off, NULL, 0) < 0) { off = len; break; }
                if (off + 4 > (size_t)len) { off = len; break; }
                off += 4;
            }
            uint16_t total = an + ns + ar;
            for (uint16_t i = 0; i < total && off + 10 <= (size_t)len; i++) {
                char rrname[128];
                if (mdns_decode_name(buf, len, &off, rrname, sizeof(rrname)) < 0) break;
                if (off + 10 > (size_t)len) break;
                uint16_t type = (buf[off] << 8) | buf[off+1];
                /* uint16_t klass = (buf[off+2] << 8) | buf[off+3]; */
                uint16_t rdlen = (buf[off+8] << 8) | buf[off+9];
                off += 10;
                if (off + rdlen > (size_t)len) break;
                LOG_DBG("mDNS: RR name=%s type=%u rdlen=%u", rrname, type, rdlen);
                if (type == 12 /* PTR */) {
                    size_t off2 = off;
                    char inst[160];
                    if (mdns_decode_name(buf, len, &off2, inst, sizeof(inst)) == 0) {
                        /* Only consider PTR for _mqtt._tcp.local to avoid noise */
                        if (strncasecmp(rrname, "_mqtt._tcp.local", sizeof(rrname)) == 0) {
                            if (instance_name[0] == '\0') {
                                strncpy(instance_name, inst, sizeof(instance_name)-1);
                                LOG_INF("mDNS: PTR instance %s", instance_name);
                            }
                        }
                    }
                }
                if (type == 33 /* SRV */ && rdlen >= 6) {
                    uint16_t port = (buf[off+4] << 8) | buf[off+5];
                    size_t off2 = off + 6;
                    char target[128];
                    if (mdns_decode_name(buf, len, &off2, target, sizeof(target)) == 0) {
                        strncpy(target_host, target, sizeof(target_host)-1);
                        target_port = port;
                        LOG_INF("mDNS: SRV %s port %u", target_host, target_port);
                    }
                } else if (type == 1 /* A */ && rdlen == 4) {
                    if (target_host[0] == '\0' || strcmp(rrname, target_host) == 0) {
                        memcpy(&a_addr, &buf[off], 4);
                        strncpy(a_for, rrname, sizeof(a_for)-1);
                        char ipbuf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &a_addr, ipbuf, sizeof(ipbuf));
                        LOG_INF("mDNS: A %s -> %s", a_for, ipbuf);
                    }
                }
                off += rdlen;
            }
            if (target_host[0] && a_for[0] && strcmp(target_host, a_for) == 0 && a_addr.s_addr != 0 && target_port != 0) {
                out_addr->sin_family = AF_INET;
                out_addr->sin_addr = a_addr;
                out_addr->sin_port = htons(target_port);
                *out_port = target_port;
                close(s);
                return 0;
            }
            /* If we only have instance name, actively resolve SRV next */
            if (instance_name[0] && target_host[0] == 0) {
                int rs = mdns_send_query(s, instance_name, 33 /* SRV */);
                if (rs >= 0) {
                    LOG_INF("mDNS: sent SRV query for %s (bytes=%d)", instance_name, rs);
                }
            }
            /* If we have target host and port but no A yet, actively resolve A */
            if (target_host[0] && a_addr.s_addr == 0) {
                int ra = mdns_send_query(s, target_host, 1 /* A */);
                if (ra >= 0) {
                    LOG_INF("mDNS: sent A query for %s (bytes=%d)", target_host, ra);
                }
            }
        }
    }
    close(s);
    return -ETIMEDOUT;
}

static int mqtt_broker_connect(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    int ret;

    LOG_INF("Discovering MQTT broker via DNS-SD: _mqtt._tcp.local");

    /* First try: true mDNS browse */
    struct sockaddr_in mdns_found;
    uint16_t mdns_port = 0;
    memset(&mdns_found, 0, sizeof(mdns_found));
    ret = mdns_browse_mqtt(&mdns_found, &mdns_port, 10000);
    if (ret == 0) {
        memcpy(broker4, &mdns_found, sizeof(struct sockaddr_in));
        char addr_str_m[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &broker4->sin_addr, addr_str_m, sizeof(addr_str_m));
        LOG_INF("mDNS discovered broker at %s:%u", addr_str_m, ntohs(broker4->sin_port));
        goto do_mqtt_connect;
    }

    // Reset DNS state
    dns_result = -1;
    resolved_port = 0;
    memset(service_instance, 0, sizeof(service_instance));
    k_sem_reset(&dns_service_sem);
    k_sem_reset(&dns_addr_sem);
    
    // Use Zephyr DNS resolver API for DNS-SD service discovery
    // Initialize a DNS context with mDNS multicast server (224.0.0.251:5353)
    // Initialize persistent mDNS server sockaddr and servers array if not yet configured
    if (mdns_sa_global.sin_family != AF_INET) {
        mdns_sa_global.sin_family = AF_INET;
        mdns_sa_global.sin_port = htons(5353);
        mdns_sa_global.sin_addr.s_addr = htonl(0xE00000FB); // 224.0.0.251

        mdns_servers_sa[0] = (const struct sockaddr *)&mdns_sa_global;
        mdns_servers_sa[1] = NULL;
    }

    ret = dns_resolve_init(&dns_ctx, NULL, mdns_servers_sa);
    if (ret < 0) {
        LOG_ERR("DNS context init (mDNS) failed: %d", ret);
        return ret;
    }
    
    LOG_DBG("Starting DNS-SD query for _mqtt._tcp.local");
    
    uint16_t dns_id = 0;
    // Browse service instances for MQTT over TCP in local domain
    ret = dns_resolve_service(&dns_ctx, "_mqtt._tcp.local", &dns_id, 
                              dns_sd_result_cb, NULL, 10000);
    if (ret < 0) {
        LOG_ERR("Failed to start DNS-SD query: %d (%s)", ret, strerror(-ret));
        return ret;
    }
    
    LOG_DBG("DNS-SD query started, waiting for instances...");
    
    // Wait for DNS-SD browse to finish (max 10 seconds)
    ret = k_sem_take(&dns_service_sem, K_SECONDS(10));
    if (ret < 0 || service_instance[0] == '\0') {
        LOG_ERR("DNS-SD browse failed or no instances found; trying hostname fallbacks");
        dns_resolve_cancel(&dns_ctx, dns_id);

        const char *fallback_hosts[] = { MQTT_BROKER_HOSTNAME, "fedora.local" };
        bool resolved = false;
        for (size_t i = 0; i < ARRAY_SIZE(fallback_hosts) && !resolved; i++) {
            uint16_t a_id = 0;
            dns_result = -1;
            k_sem_reset(&dns_addr_sem);
            LOG_INF("Fallback A lookup: %s", fallback_hosts[i]);
            int r = dns_resolve_name(&dns_ctx, fallback_hosts[i], DNS_QUERY_TYPE_A, &a_id, dns_addr_result_cb, NULL, 5000);
            if (r < 0) {
                LOG_WRN("Failed to start A query for %s: %d", fallback_hosts[i], r);
                continue;
            }
            r = k_sem_take(&dns_addr_sem, K_SECONDS(5));
            if (r == 0 && dns_result == 0) {
                resolved = true;
                break;
            }
        }

        if (!resolved) {
            /* As a last resort, allow a static IP fallback if configured */
#ifdef MQTT_BROKER_STATIC_IP
            if (strlen(MQTT_BROKER_STATIC_IP) > 0) {
                struct in_addr ip;
                if (inet_pton(AF_INET, MQTT_BROKER_STATIC_IP, &ip) == 1) {
                    memset(&resolved_addr, 0, sizeof(resolved_addr));
                    resolved_addr.sin_family = AF_INET;
                    resolved_addr.sin_addr = ip;
                    dns_result = 0;
                    resolved = true;
                    LOG_WRN("Using statically configured MQTT broker IP: %s", MQTT_BROKER_STATIC_IP);
                } else {
                    LOG_WRN("MQTT_BROKER_STATIC_IP is set but invalid: %s", MQTT_BROKER_STATIC_IP);
                }
            }
#endif
        }

        if (!resolved) {
            dns_resolve_close(&dns_ctx);
            return -ENOENT;
        }
        // Set broker (use discovered A address), default port
        memcpy(broker4, &resolved_addr, sizeof(struct sockaddr_in));
        broker4->sin_port = htons(MQTT_BROKER_PORT);
        char addr_str_fb[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &broker4->sin_addr, addr_str_fb, sizeof(addr_str_fb));
        LOG_INF("Using fallback broker at %s:%d", addr_str_fb, ntohs(broker4->sin_port));
        dns_resolve_close(&dns_ctx);
        goto do_mqtt_connect;
    }

    // Resolve the discovered service instance; resolver should return AF_INET with address
    uint16_t svc_dns_id = 0;
    dns_result = -1;
    resolved_port = 0;
    memset(srv_target_host, 0, sizeof(srv_target_host));
    k_sem_reset(&dns_svc_sem);

    LOG_INF("Resolving service instance: %s", service_instance);
    ret = dns_resolve_service(&dns_ctx, service_instance, &svc_dns_id, dns_service_resolve_cb, NULL, 10000);
    if (ret < 0) {
        LOG_ERR("Failed to start service resolve: %d (%s)", ret, strerror(-ret));
        dns_resolve_close(&dns_ctx);
        return ret;
    }

    ret = k_sem_take(&dns_svc_sem, K_SECONDS(10));
    if (ret < 0 || dns_result < 0) {
        LOG_ERR("Service instance resolution failed: %d", dns_result);
        dns_resolve_cancel(&dns_ctx, svc_dns_id);
        dns_resolve_close(&dns_ctx);
        return -ENOENT;
    }

    // Done with DNS context
    dns_resolve_close(&dns_ctx);

    // Set broker (use discovered port or default 1883)
    memcpy(broker4, &resolved_addr, sizeof(struct sockaddr_in));
    broker4->sin_port = htons(resolved_port ? resolved_port : MQTT_BROKER_PORT);
    
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &broker4->sin_addr, addr_str, sizeof(addr_str));
    LOG_INF("MQTT broker discovered at %s:%d", addr_str, ntohs(broker4->sin_port));

do_mqtt_connect:
    mqtt_client_init(&client_ctx);

    client_ctx.broker = &broker;
    client_ctx.evt_cb = mqtt_evt_handler;
    client_ctx.client_id.utf8 = (uint8_t *)MQTT_CLIENT_ID;
    client_ctx.client_id.size = strlen(MQTT_CLIENT_ID);
    client_ctx.password = NULL;
    client_ctx.user_name = NULL;
    client_ctx.protocol_version = MQTT_VERSION_3_1_1;
    client_ctx.rx_buf = rx_buffer;
    client_ctx.rx_buf_size = sizeof(rx_buffer);
    client_ctx.tx_buf = tx_buffer;
    client_ctx.tx_buf_size = sizeof(tx_buffer);
    client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

    ret = mqtt_connect(&client_ctx);
    if (ret < 0) {
        LOG_ERR("MQTT connect failed: %d", ret);
        return ret;
    }

    fds.fd = client_ctx.transport.tcp.sock;
    fds.events = POLLIN;

    LOG_INF("Connected to MQTT broker (TCP established, awaiting CONNACK)");
    return 0;
}

int app_mqtt_init(void)
{
    LOG_INF("MQTT client initialized");
    return 0;
}

static void subscribe_to_topics(void)
{
    int ret;

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subscriptions[i].active) {
            struct mqtt_topic sub_topic = {
                .topic.utf8 = (uint8_t *)subscriptions[i].topic,
                .topic.size = strlen(subscriptions[i].topic),
                .qos = MQTT_QOS_1_AT_LEAST_ONCE
            };

            struct mqtt_subscription_list sub_list = {
                .list = &sub_topic,
                .list_count = 1,
                .message_id = sys_rand32_get()
            };

            ret = mqtt_subscribe(&client_ctx, &sub_list);
            if (ret < 0) {
                LOG_ERR("Failed to subscribe to %s: %d", subscriptions[i].topic, ret);
            } else {
                LOG_INF("Subscribed to %s", subscriptions[i].topic);
            }
        }
    }
}

void mqtt_client_thread(void *p1, void *p2, void *p3)
{
    int ret;
    const uint32_t retry_delay_sec = 15;
    bool first_attempt = true;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("MQTT client thread started");

    while (1) {
        /* Wait for network interface and carrier to be up before reconnecting */
        struct net_if *iface = network_get_interface();
        int carrier_waits = 0;
        while (!iface || !net_if_is_up(iface) || !net_if_is_carrier_ok(iface)) {
            if (carrier_waits == 0) {
                LOG_WRN("MQTT: Waiting for network interface and carrier to be up before reconnecting...");
            }
            carrier_waits++;
            k_sleep(K_SECONDS(2));
            iface = network_get_interface();
        }

        if (first_attempt) {
            k_sleep(K_SECONDS(5));
            first_attempt = false;
        } else {
            LOG_INF("Retrying connection in %u seconds...", retry_delay_sec);
            k_sleep(K_SECONDS(retry_delay_sec));
        }

        ret = mqtt_broker_connect();
        if (ret < 0) {
            LOG_ERR("Failed to connect to broker, will retry");
            continue;
        }

        // Wait for MQTT CONNACK before proceeding
        {
            uint32_t deadline = k_uptime_get_32() + 5000; /* 5s */
            while (!mqtt_connected && (int32_t)(deadline - (int32_t)k_uptime_get_32()) > 0) {
                int wait = 100;
                int pr = poll(&fds, 1, wait);
                if (pr < 0) {
                    LOG_ERR("Poll error while waiting CONNACK: %d", errno);
                    break;
                }
                mqtt_input(&client_ctx);
            }
        }

        if (!mqtt_connected) {
            LOG_ERR("Timed out waiting for MQTT CONNACK");
            (void)mqtt_disconnect(&client_ctx, 0);
            continue;
        }

        LOG_INF("MQTT connection established (CONNACK received)");

        // Subscribe to all registered topics now that we're connected
        subscribe_to_topics();

        /* Publish online status to help verify connectivity */
        {
            char buf[96];
            int len = snprintk(buf, sizeof(buf), "{\"status\":\"online\",\"timestamp\":%u}", k_uptime_get_32());
            if (len > 0) {
                int pr = app_mqtt_publish("chess/system/online", buf, (uint32_t)len);
                if (pr < 0) {
                    LOG_WRN("Failed to publish online status: %d", pr);
                } else {
                    LOG_INF("Published online status");
                }
            }
        }

        // Main MQTT processing loop
        while (mqtt_connected) {
            ret = poll(&fds, 1, mqtt_keepalive_time_left(&client_ctx));
            if (ret < 0) {
                LOG_ERR("Poll error: %d", errno);
                break;
            }

            mqtt_input(&client_ctx);

            if ((fds.revents & POLLERR) || (fds.revents & POLLNVAL)) {
                LOG_ERR("Socket error");
                break;
            }

            ret = mqtt_live(&client_ctx);
            if (ret < 0 && ret != -EAGAIN) {
                LOG_ERR("MQTT live error: %d", ret);
                break;
            }

            k_sleep(K_MSEC(100));
        }

        LOG_WRN("MQTT connection lost, attempting to reconnect");
        (void)mqtt_disconnect(&client_ctx, 0);
        mqtt_connected = false;
    }
}

int app_mqtt_publish(const char *topic, const char *payload, uint32_t payload_len)
{
    struct mqtt_publish_param param;

    if (!mqtt_connected) {
        return -ENOTCONN;
    }

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)payload;
    param.message.payload.len = payload_len;
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    return mqtt_publish(&client_ctx, &param);
}

int app_mqtt_subscribe(const char *topic, mqtt_message_callback_t callback)
{
    int slot = -1;

    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (!subscriptions[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        LOG_ERR("No subscription slots available");
        return -ENOMEM;
    }

    strncpy(subscriptions[slot].topic, topic, sizeof(subscriptions[slot].topic) - 1);
    subscriptions[slot].callback = callback;
    subscriptions[slot].active = true;

    if (mqtt_connected) {
        struct mqtt_topic sub_topic = {
            .topic.utf8 = (uint8_t *)subscriptions[slot].topic,
            .topic.size = strlen(subscriptions[slot].topic),
            .qos = MQTT_QOS_1_AT_LEAST_ONCE
        };

        struct mqtt_subscription_list sub_list = {
            .list = &sub_topic,
            .list_count = 1,
            .message_id = sys_rand32_get()
        };

        int ret = mqtt_subscribe(&client_ctx, &sub_list);
        if (ret < 0) {
            LOG_ERR("Failed to subscribe to %s: %d", topic, ret);
            subscriptions[slot].active = false;
            return ret;
        }

        LOG_INF("Subscribed to %s", topic);
    }

    return 0;
}

bool app_mqtt_is_connected(void)
{
    return mqtt_connected;
}
