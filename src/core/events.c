#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(events, LOG_LEVEL_INF);

#include "core/events.h"

#define EV_QUEUE_LEN 16
K_MSGQ_DEFINE(ev_q, sizeof(event_t), EV_QUEUE_LEN, 4);

int events_init(void) {
    // nothing needed now; reserved for future
    return 0;
}

int events_post(const event_t *ev, k_timeout_t timeout) {
    return k_msgq_put(&ev_q, ev, timeout);
}

int events_get(event_t *out, k_timeout_t timeout) {
    return k_msgq_get(&ev_q, out, timeout);
}