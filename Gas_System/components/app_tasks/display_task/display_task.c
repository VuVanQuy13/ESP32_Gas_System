#include "display_task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Ham va du lieu phu chi dung trong tac vu nay. */
static void dong(char ra[17], const char *chu)
{
    size_t n = strlen(chu);
    if (n > 16)
        n = 16;
    memset(ra, ' ', 16);
    memcpy(ra, chu, n);
    ra[16] = '\0';
}

static esp_err_t ht_tao_khung(const dk_ban_chup *b, uint64_t now, lcd_khung *k)
{
    if (!b || !k || !b->trang_thai.da_khoi_tao)
        return ESP_ERR_INVALID_ARG;
    const dk_trang_thai *s = &b->trang_thai;
    char trai[24], duoi[32], tren[32];
    bool cu = now < s->hien_tai_ms || now - s->hien_tai_ms >= 1000;
    bool mau_cu = s->da_co_mau && (now < s->luc_lay_mau_ms ||
                                      now - s->luc_lay_mau_ms >= s->cau_hinh.han_du_lieu_ms);
    if (cu)
        snprintf(trai, sizeof(trai), "MAT DONGBO");
    else if (s->tinh_trang_cam_bien == DK_LOI_CAM_BIEN || mau_cu)
        snprintf(trai, sizeof(trai), "LOI CB");
    else if (s->tinh_trang_cam_bien != DK_SAN_SANG)
        snprintf(trai, sizeof(trai), "ON DINH");
    else if (!s->da_co_mau || s->mau < 0 || s->mau > 4095)
        snprintf(trai, sizeof(trai), "LOI CB");
    else
        snprintf(trai,
            sizeof(trai),
            s->cau_hinh_hop_le && s->mau >= s->nguong ? "VUOT %4d" : "ADC:%4d",
            s->mau);
    snprintf(tren,
        sizeof(tren),
        "%c R:%c %s",
        s->che_do == DK_TU_DONG ? 'A' : 'M',
        cu || b->loi_dau_ra != ESP_OK ? '?' : (s->relay_bat ? '1' : '0'),
        trai);
    if (cu)
        snprintf(duoi, sizeof(duoi), "CHO DU LIEU MOI");
    else if (s->trang_thai_chinh == DK_DANG_LUU)
        snprintf(duoi,
            sizeof(duoi),
            s->chua_ro_ket_qua_luu ? "LUU CHUA RO" : "DANG LUU:%4d",
            s->ban_nhap);
    else if (s->trang_thai_chinh == DK_DANG_CHINH)
        snprintf(duoi, sizeof(duoi), b->loi_luu ? "LOI LUU:%4d" : "CHINH:%4d", s->ban_nhap);
    else if (!s->cau_hinh_hop_le)
        snprintf(duoi, sizeof(duoi), "CAN DAT NGUONG");
    else
        snprintf(duoi, sizeof(duoi), "NGUONG:%4d", s->nguong);
    dong(k->dong[0], tren);
    dong(k->dong[1], duoi);
    return ESP_OK;
}

struct ht_tac_vu {
    dk_dich_vu *dich_vu;
    ht_thiet_lap c;
    lcd_khung cu;
    bool da_xuat, bi_loi;
    uint64_t luc_thu_cu;
    TaskHandle_t tac_vu;
};

static esp_err_t ht_chay_mot_luot(ht_tac_vu *t)
{
    if (!t)
        return ESP_ERR_INVALID_ARG;
    uint64_t now = (uint64_t)(esp_timer_get_time() / 1000);

    /* Lỗi I2C: giãn retry một giây, mỗi lần lấy lại snapshot mới nhất. */
    if (t->bi_loi && now >= t->luc_thu_cu && now - t->luc_thu_cu < 1000)
        return ESP_ERR_TIMEOUT;
    dk_ban_chup b;
    esp_err_t loi = dk_doc_lcd(t->dich_vu, &b);
    if (loi != ESP_OK)
        return loi;
    lcd_khung moi;
    loi = ht_tao_khung(&b, now, &moi);
    if (loi != ESP_OK)
        return loi;
    if (t->da_xuat && !t->bi_loi && memcmp(&moi, &t->cu, sizeof(moi)) == 0)
        return ESP_OK;
    t->luc_thu_cu = now;
    loi = t->c.xuat(&moi, t->c.ngu_canh);
    if (loi != ESP_OK) {
        t->bi_loi = true;
        ESP_LOGE("hien_thi", "su_kien=loi_lcd loi=%d", loi);
        return loi;
    }

    if (t->bi_loi)
        ESP_LOGI("hien_thi", "su_kien=lcd_phuc_hoi");
    t->bi_loi = false;
    t->da_xuat = true;
    t->cu = moi;
    return ESP_OK;
}

static void vong_lap(void *ctx)
{
    for (;;) {
        esp_err_t loi = ht_chay_mot_luot(ctx);
        if (loi != ESP_OK && loi != ESP_ERR_TIMEOUT)
            ESP_LOGW("hien_thi", "su_kien=cap_nhat_that_bai loi=%d", loi);
        vTaskDelay(pdMS_TO_TICKS(200) > 0 ? pdMS_TO_TICKS(200) : 1);
    }
}

esp_err_t ht_bat_dau(ht_tac_vu **ra, dk_dich_vu *d, const ht_thiet_lap *c)
{
    if (!ra || !d || !c || !c->xuat)
        return ESP_ERR_INVALID_ARG;
    *ra = NULL;
    ht_tac_vu *t = calloc(1, sizeof(*t));
    if (!t)
        return ESP_ERR_NO_MEM;
    t->dich_vu = d;
    t->c = *c;
    if (xTaskCreate(vong_lap, "hien_thi", 3072, t, 2, &t->tac_vu) != pdPASS) {
        free(t);
        return ESP_ERR_NO_MEM;
    }
    *ra = t;
    return ESP_OK;
}
