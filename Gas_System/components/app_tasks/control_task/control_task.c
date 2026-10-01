#include "control_task.h"

#include <inttypes.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define SO_CHO_LENH 16
#define NGAN_XEP_BYTE 4096
#define UU_TIEN 5
#define CHO_TOI_DA_MS 100

static const char *NHAN = "dieu_khien";

static void kiem_tra_noi_bo(BaseType_t ket_qua)
{
    if (ket_qua != pdPASS) {
        ESP_LOGE(NHAN, "su_kien=vi_pham_bat_bien_hang_doi");
        abort();
    }
}

typedef struct {
    uint64_t ma_lenh;
    dk_su_kien su_kien;
    bool la_nut;
    dk_nut nut;
    bool tu_web;
    uint64_t luc_nhan, phien_dieu_khien;
} dk_goi_tin;

struct dk_dich_vu {
    dk_trang_thai trang_thai;
    dk_thiet_lap_tac_vu thiet_lap;
    QueueHandle_t dau_vao, phan_hoi, lcd, web, viec_luu, ket_qua_luu;
    SemaphoreHandle_t cho_phan_hoi;
    TaskHandle_t tac_vu;
    uint64_t phien_ban;
    uint64_t phien_dieu_khien;
    esp_err_t loi_dau_ra;
    bool loi_luu;
};

static uint64_t lay_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void phat_ban_chup(dk_dich_vu *d)
{
    dk_ban_chup ban = {
        .phien_ban = ++d->phien_ban,
        .trang_thai = d->trang_thai,
        .loi_dau_ra = d->loi_dau_ra,
        .loi_luu = d->loi_luu,
        .phien_dieu_khien = d->phien_dieu_khien
    };

    /* Queue có đúng một phần tử: luôn giữ bản mới nhất. */
    kiem_tra_noi_bo(xQueueOverwrite(d->lcd, &ban));
    kiem_tra_noi_bo(xQueueOverwrite(d->web, &ban));
}

/* -1: chua du dieu kien so sanh, 0: duoi nguong, 1: cham/vuot nguong. */
static int muc_canh_bao(const dk_trang_thai *s)
{
    if (!s->cau_hinh_hop_le || !s->da_co_mau || s->tinh_trang_cam_bien != DK_SAN_SANG)
        return -1;
    return s->mau >= s->nguong;
}

static dk_ket_qua xu_ly(dk_dich_vu *d, const dk_su_kien *su_kien, uint64_t ma_lenh)
{
    dk_ket_qua k;
    int canh_bao_cu = muc_canh_bao(&d->trang_thai);
    dk_che_do che_do_cu = d->trang_thai.che_do;
    bool relay_cu = d->trang_thai.relay_bat;
    dk_tinh_trang_cam_bien cam_bien_cu = d->trang_thai.tinh_trang_cam_bien;
    dk_trang_thai_chinh chinh_cu = d->trang_thai.trang_thai_chinh;
    bool luu_chua_ro_cu = d->trang_thai.chua_ro_ket_qua_luu;
    dk_ma_ket_qua ma = dk_xu_ly_su_kien(&d->trang_thai, su_kien, lay_ms(), &k);

    /* Timeout có thể đổi đầu ra kể cả khi lệnh bị từ chối. */
    if (k.relay_thay_doi || d->loi_dau_ra != ESP_OK) {
        d->loi_dau_ra = d->thiet_lap.xuat_relay(k.relay_bat, d->thiet_lap.ngu_canh);
        if (d->loi_dau_ra != ESP_OK)
            ESP_LOGE(NHAN, "su_kien=loi_gpio ma=%d muc=%d", d->loi_dau_ra, k.relay_bat);
    }

    if (k.relay_thay_doi && d->loi_dau_ra == ESP_OK)
        ESP_LOGI(NHAN,
            "su_kien=xuat_relay yeu_cau=%s che_do=%s",
            k.relay_bat ? "BAT" : "TAT",
            k.che_do == DK_TU_DONG ? "AUTO" : "MANUAL");
    if (k.can_luu) {
        dk_viec_luu viec = { d->trang_thai.ma_khoi_dong, k.ma_lan_luu, k.nguong_can_luu };
        if (xQueueSend(d->viec_luu, &viec, 0) != pdPASS) {
            /* Chưa giao storage nên biết chắc yêu cầu mới chưa được ghi. */
            ESP_LOGE(NHAN, "su_kien=hang_luu_day id=%" PRIu64, k.ma_lan_luu);
            dk_su_kien loi = {
                .loai = DK_LUU_THAT_BAI,
                .ma_khoi_dong = d->trang_thai.ma_khoi_dong,
                .ma_lan_luu = k.ma_lan_luu
            };
            (void)dk_xu_ly_su_kien(&d->trang_thai, &loi, lay_ms(), &k);
            k.su_kien = su_kien->loai;
            ma = k.ma_ket_qua;
        }
    }

    if (che_do_cu != d->trang_thai.che_do || relay_cu != d->trang_thai.relay_bat) {
        if (d->phien_dieu_khien == UINT64_MAX) {
            ESP_LOGE(NHAN, "su_kien=het_phien_dieu_khien");
            abort();
        }
        ++d->phien_dieu_khien;
    }

    if (ma == DK_LOI_LUU_TRU)
        d->loi_luu = true;
    else if (ma == DK_THANH_CONG &&
             (su_kien->loai == DK_BAT_DAU_CHINH || su_kien->loai == DK_YEU_CAU_LUU ||
                 su_kien->loai == DK_LUU_THANH_CONG))
        d->loi_luu = false;
    int canh_bao_moi = muc_canh_bao(&d->trang_thai);
    if (canh_bao_cu != canh_bao_moi)
        ESP_LOGI(NHAN,
            "su_kien=canh_bao trang_thai=%s mau=%d nguong=%d",
            canh_bao_moi < 0 ? "CHUA_DU_DIEU_KIEN"
            : canh_bao_moi   ? "VUOT_NGUONG"
                             : "DUOI_NGUONG",
            d->trang_thai.mau,
            d->trang_thai.nguong);
    if ((su_kien->loai != DK_KIEM_TRA_THOI_GIAN && su_kien->loai != DK_MAU_MOI) ||
        k.relay_thay_doi || cam_bien_cu != k.tinh_trang_cam_bien ||
        chinh_cu != k.trang_thai_chinh || luu_chua_ro_cu != k.chua_ro_ket_qua_luu) {
        ESP_LOGI(NHAN,
            "lenh=%" PRIu64
            " su_kien=%d ket_qua=%d che_do=%s cam_bien=%s relay=%s loi_dau_ra=%d chinh=%s "
            "luu_chua_ro=%d nguong=%d ban_nhap=%d cau_hinh_hop_le=%d",
            ma_lenh,
            su_kien->loai,
            ma,
            k.che_do == DK_TU_DONG ? "AUTO" : "MANUAL",
            k.tinh_trang_cam_bien == DK_SAN_SANG       ? "SAN_SANG"
            : k.tinh_trang_cam_bien == DK_LOI_CAM_BIEN ? "LOI"
                                                       : "DANG_ON_DINH",
            k.relay_bat ? "BAT" : "TAT",
            d->loi_dau_ra,
            k.trang_thai_chinh == DK_KHONG_CHINH  ? "KHONG_CHINH"
            : k.trang_thai_chinh == DK_DANG_CHINH ? "DANG_CHINH"
                                                  : "DANG_LUU",
            k.chua_ro_ket_qua_luu,
            d->trang_thai.nguong,
            d->trang_thai.ban_nhap,
            d->trang_thai.cau_hinh_hop_le);
    }

    return k;
}

static dk_su_kien dich_nut(const dk_trang_thai *s, dk_nut nut)
{
    dk_su_kien e = {
        .ma_khoi_dong = s->ma_khoi_dong,
        .ma_phien_chinh = s->ma_phien_chinh,
        .phien_ban_mong_doi = s->phien_ban_nhap
    };
    if (nut == DK_NUT_CHE_DO) {
        e.loai = DK_DAT_CHE_DO;
        e.gia_tri = s->che_do == DK_TU_DONG ? DK_THU_CONG : DK_TU_DONG;
    } else if (nut == DK_NUT_CHINH_LUU) {
        e.loai = s->trang_thai_chinh == DK_KHONG_CHINH ? DK_BAT_DAU_CHINH : DK_YEU_CAU_LUU;
    } else if (s->trang_thai_chinh != DK_KHONG_CHINH) {
        e.loai = DK_DAT_BAN_NHAP;

        /* Bước 10 đơn vị thử nghiệm; chặn ở biên, tránh tràn số int. */
        int64_t moi = (int64_t)s->ban_nhap + (nut == DK_NUT_BAT_TANG ? 10 : -10);
        if (moi > s->cau_hinh.nguong_lon_nhat)
            moi = s->cau_hinh.nguong_lon_nhat;
        if (moi < s->cau_hinh.nguong_nho_nhat)
            moi = s->cau_hinh.nguong_nho_nhat;
        e.gia_tri = (int)moi;
    } else {
        e.loai = DK_DAT_RELAY;
        e.gia_tri = nut == DK_NUT_BAT_TANG;
    }

    return e;
}

esp_err_t dk_gui_nut(dk_dich_vu *d, dk_nut nut)
{
    if (!d || nut < DK_NUT_BAT_TANG || nut > DK_NUT_CHINH_LUU)
        return ESP_ERR_INVALID_ARG;
    dk_goi_tin goi = {
        .la_nut = true,
        .nut = nut
    };
    if (xQueueSend(d->dau_vao, &goi, 0) != pdPASS) {
        ESP_LOGW(NHAN, "su_kien=bo_nut_hang_day nut=%d", nut);
        return ESP_ERR_TIMEOUT;
    }
    xTaskNotifyGive(d->tac_vu);
    return ESP_OK;
}

static bool dk_chay_mot_luot(dk_dich_vu *d)
{
    if (d == NULL)
        return false;

    /* Mỗi lượt hữu hạn: kiểm tra thời gian, một kết quả lưu, một đầu vào.
     * Flood lệnh không được làm mất bước kiểm tra timeout. */
    dk_su_kien tick = {
        .loai = DK_KIEM_TRA_THOI_GIAN,
        .ma_khoi_dong = d->trang_thai.ma_khoi_dong
    };
    (void)xu_ly(d, &tick, 0);
    bool co_viec = false;
    dk_su_kien luu;
    if (xQueueReceive(d->ket_qua_luu, &luu, 0) == pdPASS) {
        (void)xu_ly(d, &luu, 0);
        co_viec = true;
    }
    dk_goi_tin goi;
    if (xQueueReceive(d->dau_vao, &goi, 0) == pdPASS) {
        if (goi.la_nut)
            goi.su_kien = dich_nut(&d->trang_thai, goi.nut);
        dk_phan_hoi phan = {
            .ma_lenh = goi.ma_lenh
        };
        bool doi_dau_ra = goi.su_kien.loai == DK_DAT_CHE_DO || goi.su_kien.loai == DK_DAT_RELAY;
        uint64_t now = lay_ms();
        if (goi.tu_web && (now < goi.luc_nhan || now - goi.luc_nhan >= 1000 ||
                              (doi_dau_ra && goi.phien_dieu_khien != d->phien_dieu_khien))) {
            phan.ket_qua = xu_ly(d, &tick, goi.ma_lenh);
            phan.ket_qua.su_kien = goi.su_kien.loai;
            phan.ket_qua.ma_ket_qua = DK_DU_LIEU_CU;
            ESP_LOGW(NHAN, "su_kien=tu_choi_lenh_web_cu lenh=%" PRIu64, goi.ma_lenh);
        } else
            phan.ket_qua = xu_ly(d, &goi.su_kien, goi.ma_lenh);
        if (goi.tu_web)
            ESP_LOGI(NHAN,
                "nguon=web lenh=%" PRIu64 " su_kien=%d ket_qua=%d",
                goi.ma_lenh,
                goi.su_kien.loai,
                phan.ket_qua.ma_ket_qua);
        if (goi.la_nut)
            ESP_LOGI(NHAN,
                "nguon=nut nut=%d ket_qua=%d ban_nhap=%d nguong=%d",
                goi.nut,
                phan.ket_qua.ma_ket_qua,
                d->trang_thai.ban_nhap,
                d->trang_thai.nguong);
        phan.loi_dau_ra = d->loi_dau_ra;
        if (goi.ma_lenh != 0) {
            /* Producer đã giữ một chỗ trước khi nhận lệnh. */
            kiem_tra_noi_bo(xQueueSend(d->phan_hoi, &phan, 0));
        }
        co_viec = true;
    }
    phat_ban_chup(d);
    return co_viec;
}

static TickType_t dk_so_tick_cho(const dk_dich_vu *d)
{
    uint64_t hien_tai = lay_ms();
    uint64_t han = dk_han_ke_tiep(&d->trang_thai);
    uint64_t cho_ms = han <= hien_tai ? 0 : han - hien_tai;
    if (cho_ms > CHO_TOI_DA_MS)
        cho_ms = CHO_TOI_DA_MS;

    /* Làm tròn lên một tick: không quay vòng bận khi còn dưới một tick. */
    return (TickType_t)((cho_ms * configTICK_RATE_HZ + 999) / 1000);
}

static void vong_lap(void *tham_so)
{
    dk_dich_vu *d = tham_so;
    for (;;) {
        if (dk_chay_mot_luot(d)) {
            /* Nhường CPU một tick khi có tải liên tục, tránh làm đói idle. */
            vTaskDelay(1);
        } else {
            (void)ulTaskNotifyTake(pdTRUE, dk_so_tick_cho(d));
        }
    }
}

static void don_khoi_tao_loi(dk_dich_vu *d)
{
    QueueHandle_t hang[] = { d->dau_vao, d->phan_hoi, d->lcd, d->web, d->viec_luu, d->ket_qua_luu };
    for (unsigned i = 0; i < sizeof(hang) / sizeof(hang[0]); ++i)
        if (hang[i] != NULL)
            vQueueDelete(hang[i]);
    if (d->cho_phan_hoi != NULL)
        vSemaphoreDelete(d->cho_phan_hoi);
    free(d);
}

esp_err_t dk_bat_dau(dk_dich_vu **ra, const dk_thiet_lap_tac_vu *c)
{
    if (ra == NULL || c == NULL || c->xuat_relay == NULL)
        return ESP_ERR_INVALID_ARG;
    *ra = NULL;
    dk_dich_vu *d = calloc(1, sizeof(*d));
    if (d == NULL)
        return ESP_ERR_NO_MEM;
    d->thiet_lap = *c;
    d->phien_dieu_khien = 1;
    if (dk_khoi_tao(&d->trang_thai,
            &c->loi,
            c->ma_khoi_dong,
            lay_ms(),
            c->du_lieu_luu_hop_le,
            c->nguong_da_luu) != DK_THANH_CONG) {
        free(d);
        return ESP_ERR_INVALID_ARG;
    }
    d->dau_vao = xQueueCreate(SO_CHO_LENH, sizeof(dk_goi_tin));
    d->phan_hoi = xQueueCreate(SO_CHO_LENH, sizeof(dk_phan_hoi));
    d->lcd = xQueueCreate(1, sizeof(dk_ban_chup));
    d->web = xQueueCreate(1, sizeof(dk_ban_chup));
    d->viec_luu = xQueueCreate(1, sizeof(dk_viec_luu));
    d->ket_qua_luu = xQueueCreate(1, sizeof(dk_su_kien));
    d->cho_phan_hoi = xSemaphoreCreateCounting(SO_CHO_LENH, SO_CHO_LENH);
    if (!d->dau_vao || !d->phan_hoi || !d->lcd || !d->web || !d->viec_luu || !d->ket_qua_luu ||
        !d->cho_phan_hoi) {
        don_khoi_tao_loi(d);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t loi = c->xuat_relay(false, c->ngu_canh);
    if (loi != ESP_OK) {
        don_khoi_tao_loi(d);
        return loi;
    }
    ESP_LOGI(NHAN,
        "su_kien=khoi_dong che_do=AUTO relay=TAT cam_bien=DANG_ON_DINH nguong=%d "
        "cau_hinh_hop_le=%d",
        d->trang_thai.nguong,
        d->trang_thai.cau_hinh_hop_le);
    phat_ban_chup(d);
    if (xTaskCreate(vong_lap, "dieu_khien", NGAN_XEP_BYTE, d, UU_TIEN, &d->tac_vu) != pdPASS) {
        don_khoi_tao_loi(d);
        return ESP_ERR_NO_MEM;
    }
    *ra = d;
    return ESP_OK;
}

static esp_err_t gui_lenh(dk_dich_vu *d, uint64_t id, const dk_su_kien *e, bool web, uint64_t phien)
{
    if (d == NULL || e == NULL || id == 0 || e->loai < DK_DAT_CHE_DO || e->loai > DK_YEU_CAU_LUU)
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(d->cho_phan_hoi, 0) != pdPASS) {
        ESP_LOGW(NHAN, "su_kien=het_cho_phan_hoi lenh=%" PRIu64, id);
        return ESP_ERR_TIMEOUT;
    }
    dk_goi_tin goi = {
        .ma_lenh = id,
        .su_kien = *e,
        .tu_web = web,
        .luc_nhan = lay_ms(),
        .phien_dieu_khien = phien
    };
    if (xQueueSend(d->dau_vao, &goi, 0) != pdPASS) {
        kiem_tra_noi_bo(xSemaphoreGive(d->cho_phan_hoi));
        ESP_LOGW(NHAN, "su_kien=hang_dau_vao_day lenh=%" PRIu64, id);
        return ESP_ERR_TIMEOUT;
    }
    xTaskNotifyGive(d->tac_vu);
    return ESP_OK;
}

esp_err_t dk_gui_lenh(dk_dich_vu *d, uint64_t id, const dk_su_kien *e)
{
    return gui_lenh(d, id, e, false, 0);
}

esp_err_t dk_gui_lenh_web(dk_dich_vu *d, uint64_t id, const dk_su_kien *e, uint64_t phien)
{
    return gui_lenh(d, id, e, true, phien);
}

esp_err_t dk_gui_mau(dk_dich_vu *d, const dk_su_kien *e)
{
    if (d == NULL || e == NULL || e->loai != DK_MAU_MOI)
        return ESP_ERR_INVALID_ARG;
    dk_goi_tin goi = {
        .su_kien = *e
    };
    if (xQueueSend(d->dau_vao, &goi, 0) != pdPASS) {
        ESP_LOGW(NHAN, "su_kien=mat_mau thu_tu=%" PRIu64, e->so_thu_tu);
        return ESP_ERR_TIMEOUT;
    }
    xTaskNotifyGive(d->tac_vu);
    return ESP_OK;
}

esp_err_t dk_doc_phan_hoi(dk_dich_vu *d, dk_phan_hoi *p)
{
    if (!d || !p)
        return ESP_ERR_INVALID_ARG;
    if (xQueueReceive(d->phan_hoi, p, 0) != pdPASS)
        return ESP_ERR_NOT_FOUND;
    kiem_tra_noi_bo(xSemaphoreGive(d->cho_phan_hoi));
    return ESP_OK;
}

esp_err_t dk_doc_lcd(dk_dich_vu *d, dk_ban_chup *b)
{
    if (!d || !b)
        return ESP_ERR_INVALID_ARG;
    return xQueuePeek(d->lcd, b, 0) == pdPASS ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t dk_doc_web(dk_dich_vu *d, dk_ban_chup *b)
{
    if (!d || !b)
        return ESP_ERR_INVALID_ARG;
    return xQueuePeek(d->web, b, 0) == pdPASS ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t dk_nhan_viec_luu(dk_dich_vu *d, dk_viec_luu *v)
{
    if (!d || !v)
        return ESP_ERR_INVALID_ARG;
    return xQueueReceive(d->viec_luu, v, 0) == pdPASS ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t dk_bao_ket_qua_luu(dk_dich_vu *d, const dk_su_kien *e)
{
    if (!d || !e || (e->loai != DK_LUU_THANH_CONG && e->loai != DK_LUU_THAT_BAI))
        return ESP_ERR_INVALID_ARG;
    if (xQueueSend(d->ket_qua_luu, e, 0) != pdPASS) {
        ESP_LOGW(NHAN, "su_kien=hang_ket_qua_luu_day id=%" PRIu64, e->ma_lan_luu);
        return ESP_ERR_TIMEOUT; /* Storage giữ kết quả và thử lại, không bỏ. */
    }
    xTaskNotifyGive(d->tac_vu);
    return ESP_OK;
}
