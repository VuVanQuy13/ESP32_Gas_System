#include "sensor_task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdlib.h>

/* Ham va du lieu phu chi dung trong tac vu nay. */
typedef struct {
    unsigned so_mau, vi_tri, tong;
    int mau[4];
} cb_bo_loc;

static bool cb_loc_mau(cb_bo_loc *l, bool thanh_cong, int tho, int *ra)
{
    if (!l || !ra)
        return false;
    if (!thanh_cong || tho <= 0 || tho >= 4095) {
        *l = (cb_bo_loc){ 0 };
        return false;
    }

    if (l->so_mau == 4)
        l->tong -= (unsigned)l->mau[l->vi_tri];
    else
        ++l->so_mau;
    l->mau[l->vi_tri] = tho;
    l->tong += (unsigned)tho;
    l->vi_tri = (l->vi_tri + 1) % 4;
    *ra = (int)(l->tong / l->so_mau);
    return true;
}

struct cb_tac_vu {
    dk_dich_vu *dich_vu;
    cb_thiet_lap thiet_lap;
    cb_bo_loc loc;
    uint64_t thu_tu, luc_doc_cu, luc_log_cu;
    bool da_log, mau_log_hop_le;
    TaskHandle_t tac_vu;
};

static uint64_t lay_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static esp_err_t cb_chay_mot_luot(cb_tac_vu *t)
{
    if (!t)
        return ESP_ERR_INVALID_ARG;
    uint64_t luc_doc = lay_ms();
    if (t->thu_tu == UINT64_MAX || (t->thu_tu && luc_doc <= t->luc_doc_cu))
        return ESP_ERR_INVALID_STATE;
    bool dut_mau = t->thu_tu && luc_doc - t->luc_doc_cu >= t->thiet_lap.han_mau_ms;
    int tho = 0, loc = 0;
    esp_err_t loi_doc = t->thiet_lap.doc_adc(&tho, t->thiet_lap.ngu_canh);
    uint64_t luc_xong = lay_ms();
    if (luc_xong < luc_doc)
        return ESP_ERR_INVALID_STATE;
    bool qua_han = luc_xong - luc_doc >= t->thiet_lap.han_mau_ms;

    /* Giữ timestamp trước lời gọi ADC: đọc chậm không được biến thành mẫu mới.
     * Không kéo lịch sử cũ qua lỗi, đứt mẫu hoặc lần đọc quá hạn. */
    bool hop_le = cb_loc_mau(&t->loc, loi_doc == ESP_OK && !dut_mau && !qua_han, tho, &loc);
    dk_su_kien e = {
        .loai = DK_MAU_MOI,
        .ma_khoi_dong = t->thiet_lap.ma_khoi_dong,
        .so_thu_tu = ++t->thu_tu,
        .luc_lay_mau_ms = luc_doc,
        .hop_le = hop_le,
        .gia_tri = loc
    };
    t->luc_doc_cu = luc_doc;
    esp_err_t loi_gui = dk_gui_mau(t->dich_vu, &e);
    if (loi_gui != ESP_OK)
        t->loc = (cb_bo_loc){ 0 };
    bool mau_tot = hop_le && loi_gui == ESP_OK;
    if (!t->da_log || luc_doc - t->luc_log_cu >= 1000 || mau_tot != t->mau_log_hop_le) {
        ESP_LOGI("cam_bien",
            "su_kien=do_mau thu_tu=%" PRIu64
            " tho=%d loc=%d hop_le=%d loi_adc=%d dut_mau=%d qua_han=%d loi_gui=%d don_vi=ADC",
            t->thu_tu,
            tho,
            loc,
            hop_le,
            loi_doc,
            dut_mau,
            qua_han,
            loi_gui);
        t->da_log = true;
        t->luc_log_cu = luc_doc;
        t->mau_log_hop_le = mau_tot;
    }

    /* Queue đầy: bỏ mẫu, không gửi lại mẫu cũ; sequence vẫn tăng để lõi nhận ra gap. */
    return loi_gui;
}

static void vong_lap_cam_bien(void *tham_so)
{
    cb_tac_vu *t = tham_so;
    for (;;) {
        esp_err_t loi = cb_chay_mot_luot(t);
        if (loi != ESP_OK && loi != ESP_ERR_TIMEOUT) {
            ESP_LOGE("cam_bien", "su_kien=loi_dich_vu loi=%d", loi);
            abort();
        }

        /* Chờ sau mỗi lần đọc, không đọc dồn để bù các chu kỳ đã bỏ lỡ. */
        TickType_t cho = pdMS_TO_TICKS(t->thiet_lap.chu_ky_ms);
        vTaskDelay(cho > 0 ? cho : 1);
    }
}

esp_err_t cb_bat_dau(cb_tac_vu **ra, dk_dich_vu *d, const cb_thiet_lap *c)
{
    if (!ra || !d || !c || !c->doc_adc || !c->ma_khoi_dong || c->chu_ky_ms < 10 ||
        c->chu_ky_ms > 1000 || c->han_mau_ms <= c->chu_ky_ms || c->han_mau_ms > 60000)
        return ESP_ERR_INVALID_ARG;
    *ra = NULL;
    dk_ban_chup ban;
    esp_err_t loi = dk_doc_web(d, &ban);
    if (loi != ESP_OK)
        return loi;
    if (ban.trang_thai.ma_khoi_dong != c->ma_khoi_dong ||
        ban.trang_thai.cau_hinh.han_du_lieu_ms != c->han_mau_ms)
        return ESP_ERR_INVALID_ARG;
    cb_tac_vu *t = calloc(1, sizeof(*t));
    if (!t)
        return ESP_ERR_NO_MEM;
    t->dich_vu = d;
    t->thiet_lap = *c;
    if (xTaskCreate(vong_lap_cam_bien, "cam_bien", 3072, t, 4, &t->tac_vu) != pdPASS) {
        free(t);
        return ESP_ERR_NO_MEM;
    }
    *ra = t;
    return ESP_OK;
}
