#include "subsys/comm/proto.h"
#include <cJSON.h>
#include <string.h>
#include <stdio.h>

static proto_cfg_t g_cfg;
static char topic_status_buf[96];
static char topic_cmd_buf[96];

void proto_init(const proto_cfg_t *cfg) {
    g_cfg = *cfg;
    snprintf(topic_status_buf, sizeof(topic_status_buf), "%s/status", g_cfg.base_topic);
    snprintf(topic_cmd_buf, sizeof(topic_cmd_buf), "%s/cmd", g_cfg.base_topic);
}
const char *proto_topic_status(void) { return topic_status_buf; }
const char *proto_topic_cmd(void) { return topic_cmd_buf; }

int proto_encode_status(char *buf, size_t len, const uint8_t board[8], const pos_t *pos, int last_err) {
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    for (int i=0;i<8;++i) cJSON_AddItemToArray(arr, cJSON_CreateNumber(board[i]));
    cJSON_AddItemToObject(root, "board", arr);
    cJSON_AddNumberToObject(root, "x", pos->x);
    cJSON_AddNumberToObject(root, "y", pos->y);
    cJSON_AddNumberToObject(root, "z", pos->z);
    cJSON_AddNumberToObject(root, "err", last_err);
    char *printed = cJSON_PrintUnformatted(root);
    int rc = -ENOMEM;
    if (printed) {
        rc = snprintf(buf, len, "%s", printed);
        cJSON_free(printed);
    }
    cJSON_Delete(root);
    return rc;
}

bool proto_decode_cmd(const char *json, cmd_t *out) {
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (cJSON_IsString(type) && type->valuestring) {
        if (strcmp(type->valuestring, "home")==0) {
            out->type = CMD_HOME;
        } else if (strcmp(type->valuestring, "move_to")==0) {
            out->type = CMD_MOVE_TO;
            cJSON *x=cJSON_GetObjectItem(root,"x");
            cJSON *y=cJSON_GetObjectItem(root,"y");
            cJSON *z=cJSON_GetObjectItem(root,"z");
            if (cJSON_IsNumber(x)) out->target.x = (float)x->valuedouble;
            if (cJSON_IsNumber(y)) out->target.y = (float)y->valuedouble;
            if (cJSON_IsNumber(z)) out->target.z = (float)z->valuedouble;
        } else {
            // unknown type
        }
    }
    cJSON_Delete(root);
    return out->type != CMD_NONE;
}