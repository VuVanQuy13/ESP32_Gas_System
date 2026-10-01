#include "button_task.h"
#include "buttons.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

/* Ham va du lieu phu chi dung trong tac vu nay. */
typedef struct {
    bool khoi_tao;
    uint8_t ung_vien, on_dinh, cho_nhan;
    uint64_t luc_doi[4], luc_cu;
} nut_bo_loc;

static bool nut_loc(nut_bo_loc *l, uint8_t m, uint64_t ms, uint8_t *ra)
{
    if (!l || !ra || m > 15 || (l->khoi_tao && ms < l->luc_cu))
        return false;
    *ra = 0;
    if (!l->khoi_tao) {
        l->khoi_tao = true;
        l->ung_vien = m;
        l->on_dinh = m;
        l->cho_nhan = (uint8_t)(~m & 15);
        for (unsigned i = 0; i < 4; ++i)
            l->luc_doi[i] = ms;
    }
    l->luc_cu = ms;
    for (unsigned i = 0; i < 4; ++i) {
        uint8_t bit = (uint8_t)(1U << i);
        if ((m ^ l->ung_vien) & bit) {
            l->ung_vien ^= bit;
            l->luc_doi[i] = ms;
        }

        if (((l->ung_vien ^ l->on_dinh) & bit) && ms - l->luc_doi[i] >= 30) {
            l->on_dinh ^= bit;
            if (!(l->on_dinh & bit))
                l->cho_nhan |= bit;
            else {
                if (l->cho_nhan & bit)
                    *ra |= bit;
                l->cho_nhan &= (uint8_t)~bit;
            }
        }
    }

    return true;
}

static bool da_chay;

static void vong_lap_nut(void *tham_so)
{
    nut_bo_loc loc = { 0 };
    for (;;) {
        uint8_t muc, canh;
        ESP_ERROR_CHECK(nut_doc(&muc));
        if (!nut_loc(&loc, muc, (uint64_t)(esp_timer_get_time() / 1000), &canh)) {
            ESP_LOGE("nut", "su_kien=loi_bo_loc");
            abort();
        }

        /* Nhiều cạnh cùng lượt: thứ tự cố định ON, OFF, MODE, EDIT. */
        for (unsigned i = 0; i < 4; ++i)
            if (canh & (1U << i)) {
                static const char *const ten_nut[] = {
                    "BAT_TANG", "TAT_GIAM", "AUTO_MANUAL", "CHINH_LUU"
                };
                ESP_LOGI("nut", "su_kien=nhan_on_dinh nut=%s", ten_nut[i]);
                esp_err_t loi = dk_gui_nut(tham_so, (dk_nut)i);
                if (loi != ESP_OK)
                    ESP_LOGW("nut", "su_kien=gui_that_bai nut=%u loi=%d", i, loi);
            }
        vTaskDelay(pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1);
    }
}

esp_err_t nut_bat_dau(dk_dich_vu *d)
{
    if (!d)
        return ESP_ERR_INVALID_ARG;
    if (da_chay)
        return ESP_ERR_INVALID_STATE;
    uint8_t muc;
    esp_err_t loi = nut_doc(&muc);
    if (loi != ESP_OK)
        return loi;
    if (xTaskCreate(vong_lap_nut, "doc_nut", 3072, d, 4, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    da_chay = true;
    return ESP_OK;
}
