#pragma once
#include "control_task.h"
#include <stddef.h>
#define WEB_SO_KHACH 3
#define WEB_LENH_TOI_DA 384
#define WEB_BAN_TIN_TOI_DA 1536

typedef struct {
    uint32_t id;
    uint64_t dang_cho;
    char lenh[WEB_LENH_TOI_DA + 1];
    char tra_loi[192];
} web_phien;

typedef struct {
    dk_dich_vu *dich_vu;
    uint64_t ma_noi_bo;
    web_phien khach[WEB_SO_KHACH];
} web_cong;

/* Chỉ HTTP server task sử dụng các hàm gateway, không cần mutex chéo task. */
esp_err_t web_nhan_lenh(
    web_cong *cong, unsigned khach, const char *json, char *tra_loi, size_t suc_chua);
esp_err_t web_don_phan_hoi(web_cong *cong);
esp_err_t web_tao_ban_tin(const dk_ban_chup *ban, uint64_t now, char *ra, size_t suc_chua);
