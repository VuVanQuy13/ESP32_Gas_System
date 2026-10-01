#include "storage_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdlib.h>

struct luu_tac_vu {
    dk_dich_vu *dich_vu;
    luu_kho kho;
    bool co_ket_qua;
    dk_su_kien ket_qua;
    TaskHandle_t task;
};

static esp_err_t luu_chay_mot_luot(luu_tac_vu *t)
{
    if (!t)
        return ESP_ERR_INVALID_ARG;
    if (!t->co_ket_qua) {
        dk_viec_luu viec;
        esp_err_t loi = dk_nhan_viec_luu(t->dich_vu, &viec);
        if (loi != ESP_OK)
            return loi;
        loi = luu_ghi(&t->kho, viec.nguong);
        ESP_LOGI("luu_tru",
            "su_kien=ghi_xong boot=%" PRIu64 " lan=%" PRIu64 " nguong=%d loi=%d",
            viec.ma_khoi_dong,
            viec.ma_lan_luu,
            viec.nguong,
            loi);
        t->ket_qua = (dk_su_kien){
            .loai = loi == ESP_OK ? DK_LUU_THANH_CONG : DK_LUU_THAT_BAI,
            .ma_khoi_dong = viec.ma_khoi_dong,
            .ma_lan_luu = viec.ma_lan_luu
        };
        t->co_ket_qua = true;
    }
    esp_err_t loi = dk_bao_ket_qua_luu(t->dich_vu, &t->ket_qua);
    if (loi == ESP_OK)
        t->co_ket_qua = false;

    /* Queue đầy: giữ nguyên kết quả, lượt sau chỉ gửi lại, không ghi NVS lại. */
    return loi;
}

static void vong_lap_luu(void *tham_so)
{
    for (;;) {
        esp_err_t loi = luu_chay_mot_luot(tham_so);
        if (loi != ESP_OK && loi != ESP_ERR_NOT_FOUND && loi != ESP_ERR_TIMEOUT) {
            ESP_LOGE("luu_tru", "su_kien=loi_dich_vu loi=%d", loi);
            abort();
        }
        vTaskDelay(pdMS_TO_TICKS(20) > 0 ? pdMS_TO_TICKS(20) : 1);
    }
}

esp_err_t luu_bat_dau(luu_tac_vu **ra, dk_dich_vu *d, const luu_kho *k)
{
    if (!ra || !d || !k || !k->da_thiet_lap)
        return ESP_ERR_INVALID_ARG;
    *ra = NULL;
    luu_tac_vu *t = calloc(1, sizeof(*t));
    if (!t)
        return ESP_ERR_NO_MEM;
    t->dich_vu = d;
    t->kho = *k;
    if (xTaskCreate(vong_lap_luu, "luu_tru", 3072, t, 3, &t->task) != pdPASS) {
        free(t);
        return ESP_ERR_NO_MEM;
    }
    *ra = t;
    return ESP_OK;
}
