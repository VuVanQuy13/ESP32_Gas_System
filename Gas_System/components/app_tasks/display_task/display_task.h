#pragma once
#include "control_task.h"
#include "lcd1602.h"
typedef struct ht_tac_vu ht_tac_vu;

typedef struct {
    esp_err_t (*xuat)(const lcd_khung *khung, void *ngu_canh);
    void *ngu_canh; /* Tồn tại đến reset, chỉ display_task dùng. */
} ht_thiet_lap;

esp_err_t ht_bat_dau(ht_tac_vu **ra, dk_dich_vu *dich_vu, const ht_thiet_lap *cau_hinh);
