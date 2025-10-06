#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app/app.h"
#include "core/events.h"
#include "subsys/sensors/board_state.h"
// Temporarily disabled components for chess board testing phase
// #include "subsys/motion/planner.h"
// #include "subsys/comm/mqtt_client.h"
// #include "subsys/comm/proto.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static board_state_t g_board;
static uint8_t last_board[8] = {0};
// Disabled for testing phase
// static pos_t g_pos = {0};
// static planner_t g_planner;

// =============================================================================
// DISABLED MQTT COMMAND HANDLER FOR TESTING PHASE
// =============================================================================
/*
static void on_mqtt_cmd(const cmd_t *cmd) {
    event_t ev = {.ts = k_uptime_get()};
    if (cmd->type == CMD_HOME) {
        ev.type = EV_MOVE_CMD;
        ev.u.move.from.x = ev.u.move.from.y = 0; // not used
        ev.u.move.to.x = 0; ev.u.move.to.y = 0; // signal homing via app flow
    } else if (cmd->type == CMD_MOVE_TO) {
        // Overload EV_MOVE_CMD to carry target position; or define a new EV
        // For brevity, we just plan directly here:
        planner_move_to(&g_planner, cmd->target.x, cmd->target.y, cmd->target.z);
        return;
    }
    (void)events_post(&ev, K_NO_WAIT);
}
*/

static void app_thread(void *a, void *b, void *c) {
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    LOG_INF("Starting application thread - TESTING MODE");
    LOG_INF("Only chess board detection is active");

    events_init();
    board_state_init(&g_board, 3);

    // =============================================================================
    // DISABLED MQTT AND PROTOCOL INIT FOR TESTING PHASE  
    // =============================================================================
    /*
    // MQTT brings its own thread; initialize protocol
    proto_init(&(proto_cfg_t){ .base_topic = "schach/robot/1" });
    (void)mqttc_init(&(mqttc_cfg_t){
        .client_id = "robot-1",
        .broker_host = "192.168.1.100",
        .broker_port = 1883
    }, on_mqtt_cmd);
    */

    LOG_INF("Chess board state monitoring started");
    
    // Simplified status monitoring for testing
    while (1) {
        event_t ev;
        if (events_get(&ev, K_MSEC(200)) == 0) {
            switch (ev.type) {
            case EV_MATRIX_FRAME: {
                uint8_t prev[8]; board_state_copy(&g_board, prev);
                if (board_state_update(&g_board, ev.u.frame)) {
                    uint8_t now[8]; board_state_copy(&g_board, now);
                    move_t mv;
                    if (board_state_detect_move(prev, now, &mv)) {
                        LOG_INF("Chess move detected: (%d,%d)->(%d,%d)", mv.from.x, mv.from.y, mv.to.x, mv.to.y);
                        // Log board state in testing mode (raw board data)
                        LOG_INF("Board state updated - raw data: 0x%02X%02X%02X%02X%02X%02X%02X%02X", 
                                now[0], now[1], now[2], now[3], now[4], now[5], now[6], now[7]);
                    }
                    memcpy(last_board, now, 8);
                }
            } break;
            case EV_MOTION_DONE:
                LOG_INF("Motion done event received (testing mode - no motion active)");
                break;
            default: 
                LOG_DBG("Unhandled event type: %d", ev.type);
                break;
            }
        }
        
        // =============================================================================
        // DISABLED MQTT STATUS PUBLISHING FOR TESTING PHASE  
        // =============================================================================
        /*
        // periodic status
        (void)mqttc_publish_status(last_board, &g_pos, last_err);
        */
    }
}

K_THREAD_STACK_DEFINE(app_stack, 2048);
static struct k_thread app_thr;

int app_start(void) {
    k_thread_create(&app_thr, app_stack, K_THREAD_STACK_SIZEOF(app_stack),
                    app_thread, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(12), 0, K_NO_WAIT);
    k_thread_name_set(&app_thr, "app");
    return 0;
}