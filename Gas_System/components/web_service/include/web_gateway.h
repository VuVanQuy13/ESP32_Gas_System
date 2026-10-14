#pragma once
#include "control_task.h"
#include <stddef.h>
#define WEB_SO_KHACH 3
#define WEB_LENH_TOI_DA 384
#define WEB_BAN_TIN_TOI_DA 1536

typedef struct {
    uint32_t id;
    uint64_t pending_id;
    char command[WEB_LENH_TOI_DA + 1];
    char reply[192];
} web_session;

typedef struct {
    control_service_t *service;
    uint64_t internal_id;
    web_session clients[WEB_SO_KHACH];
} web_cong;

/* Chỉ HTTP server task sử dụng các hàm gateway, không cần mutex chéo task. */
esp_err_t web_receive_command(
    web_cong *gateway, unsigned clients, const char *json, char *reply, size_t capacity);
esp_err_t web_drain_responses(web_cong *gateway);
esp_err_t web_tao_ban_tin(const control_snapshot *stored_value, uint64_t now, char *out, size_t capacity);
