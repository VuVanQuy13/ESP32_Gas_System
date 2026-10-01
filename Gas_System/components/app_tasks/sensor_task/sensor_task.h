#pragma once
#include "control_task.h"
typedef struct cb_tac_vu cb_tac_vu;

typedef struct {
    uint64_t ma_khoi_dong;
    unsigned chu_ky_ms, han_mau_ms;

    /* Callback ngắn, chỉ đọc mẫu thô. Ngu cảnh tồn tại đến reset. */
    esp_err_t (*doc_adc)(int *mau, void *ngu_canh);
    void *ngu_canh;
} cb_thiet_lap;

/* Gọi một lần sau khi tạo dịch vụ điều khiển; task sở hữu bộ lọc và sequence. */
esp_err_t cb_bat_dau(cb_tac_vu **ra, dk_dich_vu *dich_vu, const cb_thiet_lap *thiet_lap);
