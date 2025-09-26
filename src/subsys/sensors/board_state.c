#include "subsys/sensors/board_state.h"
#include <string.h>

void board_state_init(board_state_t *s, uint8_t threshold) {
    memset(s, 0, sizeof(*s));
    s->threshold = threshold ? threshold : 3;
}

bool board_state_update(board_state_t *s, const uint8_t frame[8]) {
    static uint8_t last[8] = {0};
    bool changed = false;

    for (int r = 0; r < 8; ++r) {
        if (frame[r] == last[r]) {
            if (s->accum_same[r] < 255) s->accum_same[r]++;
        } else {
            s->accum_same[r] = 0;
            last[r] = frame[r];
        }

        if (s->accum_same[r] >= s->threshold && s->stable[r] != frame[r]) {
            s->stable[r] = frame[r];
            changed = true;
        }
    }
    return changed;
}

void board_state_copy(const board_state_t *s, uint8_t out[8]) {
    memcpy(out, s->stable, 8);
}

static int popcnt8(uint8_t v) {
    v = v - ((v >> 1) & 0x55);
    v = (v & 0x33) + ((v >> 2) & 0x33);
    return (((v + (v >> 4)) & 0x0F) * 0x01);
}

bool board_state_detect_move(const uint8_t prev[8], const uint8_t curr[8], move_t *out) {
    // naive: exactly one bit turns off and one turns on
    int prev_on = 0, curr_on = 0;
    for (int r=0;r<8;++r){ prev_on += popcnt8(prev[r]); curr_on+=popcnt8(curr[r]); }

    if (prev_on != curr_on) return false;

    int from_r=-1, from_c=-1, to_r=-1, to_c=-1;
    for (int r=0;r<8;++r) {
        uint8_t diff = prev[r] ^ curr[r];
        if (!diff) continue;
        for (int c=0;c<8;++c) {
            if (!(diff & (1u<<c))) continue;
            bool was = prev[r] & (1u<<c);
            bool now = curr[r] & (1u<<c);
            if (was && !now) { from_r=r; from_c=c; }
            if (!was && now) { to_r=r; to_c=c; }
        }
    }
    if (from_r>=0 && to_r>=0) {
        out->from.y = from_r; out->from.x = from_c;
        out->to.y   = to_r;   out->to.x   = to_c;
        return true;
    }
    return false;
}