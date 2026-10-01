#include "sdkconfig.h"
#include "board_pins.h"
#include "control_task.h"
#include "esp_log.h"
#include "button_task.h"
#include "buttons.h"
#include "relay.h"
#include "storage_task.h"
#include "sensor_task.h"
#include "mq2.h"
#include "display_task.h"
#include "lcd1602_i2c.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "network.h"
#include "web_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *NHAN = "ung_dung";

#if CONFIG_DK_THU_NGHIEM
static esp_err_t xuat_lcd_gia(const lcd_khung *k, void *ctx)
{
    (void)ctx;
    ESP_LOGI("lcd_gia", "dong_1=[%s] dong_2=[%s]", k->dong[0], k->dong[1]);
    return ESP_OK;
}

static esp_err_t doc_adc_gia(int *mau, void *ngu_canh)
{
    (void)ngu_canh;

    /* Chu kỳ demo 15 giây: thấp 5 s, cao 5 s, lỗi 1 s, thấp 4 s. */
    unsigned pha = (unsigned)((esp_timer_get_time() / 1000000) % 15);
    if (pha == 10)
        return ESP_ERR_TIMEOUT;
    *mau = pha >= 5 && pha < 10 ? 900 : 400;
    return ESP_OK;
}

static esp_err_t xuat_relay_gia(bool bat, void *ngu_canh)
{
    (void)ngu_canh;
    ESP_LOGI(NHAN, "che_do=gia_lap relay=%d gpio=khong_su_dung", bat);
    return ESP_OK;
}
#else
static mq2_kenh kenh_mq2;
static lcd_i2c man_hinh;

static esp_err_t doc_adc_that(int *mau, void *ngu_canh)
{
    return mq2_doc(ngu_canh, mau);
}

static esp_err_t xuat_relay_that(bool bat, void *ngu_canh)
{
    (void)ngu_canh;
    return relay_dat(bat);
}
#endif

void app_main(void)
{
    /* Ngưỡng dùng ADC thô; thời gian ổn định thật lấy từ cấu hình bên dưới. */
    dk_thiet_lap_tac_vu thiet_lap = {
        .loi = {
            .thoi_gian_on_dinh_ms = 1000,
            .han_du_lieu_ms = 500,
            .han_chinh_ms = 30000,
            .han_luu_ms = 2000,
            .so_mau_can_thiet = 3,
            .nguong_nho_nhat = 100,
            .nguong_lon_nhat = 4000,
            .do_tre_nguong = 50,
            .nguong_nhap_ban_dau = 600
        },
        .ma_khoi_dong = 1,
        .du_lieu_luu_hop_le = false,
#if CONFIG_DK_THU_NGHIEM
        .xuat_relay = xuat_relay_gia,
#else
        .xuat_relay = xuat_relay_that,
#endif
    };

    /* Nhận dạng boot 64-bit, không phải khóa xác thực. Tạo trước khi ADC hoạt động. */
    thiet_lap.ma_khoi_dong = ((uint64_t)esp_random() << 32) | esp_random();
    if (!thiet_lap.ma_khoi_dong)
        thiet_lap.ma_khoi_dong = 1;
#if !CONFIG_DK_THU_NGHIEM
    /* 0 nghĩa chưa xác định qua tài liệu/thực nghiệm: chưa cho Ready.
     * Không dùng thời gian demo 1 giây cho MQ-2 thật. */
    thiet_lap.loi.thoi_gian_on_dinh_ms =
        CONFIG_MQ2_ON_DINH_GIAY > 0 ? (uint64_t)CONFIG_MQ2_ON_DINH_GIAY * 1000 : UINT64_MAX;
    ESP_ERROR_CHECK(relay_khoi_tao());
    ESP_LOGI(NHAN,
        "su_kien=cau_hinh_gpio mq2=%d relay=%d sda=%d scl=%d nut=%d,%d,%d,%d",
        CONFIG_GAS_GPIO_MQ2,
        CONFIG_GAS_GPIO_RELAY,
        CONFIG_GAS_GPIO_LCD_SDA,
        CONFIG_GAS_GPIO_LCD_SCL,
        CONFIG_GAS_GPIO_NUT_BAT,
        CONFIG_GAS_GPIO_NUT_TAT,
        CONFIG_GAS_GPIO_NUT_CHE_DO,
        CONFIG_GAS_GPIO_NUT_CHINH);
    ESP_ERROR_CHECK(nut_khoi_tao());
    esp_err_t loi_adc = mq2_khoi_tao(&kenh_mq2);
    if (loi_adc != ESP_OK)
        ESP_LOGE(NHAN, "su_kien=loi_khoi_tao_adc loi=%d", loi_adc);
    if (CONFIG_MQ2_ON_DINH_GIAY == 0)
        ESP_LOGW(NHAN, "su_kien=chua_cau_hinh_on_dinh_mq2 san_sang=bi_khoa");
#endif

    luu_kho kho = { 0 };
    esp_err_t loi_luu =
        luu_khoi_tao(&kho, thiet_lap.loi.nguong_nho_nhat, thiet_lap.loi.nguong_lon_nhat);
    if (loi_luu == ESP_OK)
        loi_luu = luu_doc(&kho, &thiet_lap.nguong_da_luu);
    thiet_lap.du_lieu_luu_hop_le = loi_luu == ESP_OK;
    if (loi_luu != ESP_OK)
        ESP_LOGW(NHAN, "su_kien=can_cau_hinh loi_nvs=%d", loi_luu);
    else
        ESP_LOGI(NHAN, "su_kien=khoi_phuc_nguong nguong=%d", thiet_lap.nguong_da_luu);

    dk_dich_vu *dich_vu = NULL;
    ESP_ERROR_CHECK(dk_bat_dau(&dich_vu, &thiet_lap));

    luu_tac_vu *tac_vu_luu = NULL;
    ESP_ERROR_CHECK(luu_bat_dau(&tac_vu_luu, dich_vu, &kho));

    const cb_thiet_lap cau_hinh_cb = {
        .ma_khoi_dong = thiet_lap.ma_khoi_dong,
        .chu_ky_ms = 100,
        .han_mau_ms = 500,
#if CONFIG_DK_THU_NGHIEM
        .doc_adc = doc_adc_gia,
#else
        .doc_adc = doc_adc_that,
        .ngu_canh = &kenh_mq2,
#endif
    };
    cb_tac_vu *tac_vu_cb = NULL;
    ESP_ERROR_CHECK(cb_bat_dau(&tac_vu_cb, dich_vu, &cau_hinh_cb));

    ht_thiet_lap cau_hinh_ht;
#if CONFIG_DK_THU_NGHIEM
    cau_hinh_ht = (ht_thiet_lap){
        .xuat = xuat_lcd_gia
    };
#else
    man_hinh.dia_chi = CONFIG_LCD_DIA_CHI;
#if CONFIG_LCD_TU_TIM
    man_hinh.tu_tim_dia_chi = true;
#endif
    man_hinh.so_do = lcd_so_do_mac_dinh();
#if CONFIG_LCD_DEN_MUC_CAO
    man_hinh.so_do.den_muc_cao = true;
#else
    man_hinh.so_do.den_muc_cao = false;
#endif
    cau_hinh_ht = (ht_thiet_lap){
        .xuat = lcd_i2c_xuat,
        .ngu_canh = &man_hinh
    };
#endif
    ht_tac_vu *tac_vu_ht = NULL;
    ESP_ERROR_CHECK(ht_bat_dau(&tac_vu_ht, dich_vu, &cau_hinh_ht));
#if CONFIG_DK_THU_NGHIEM
    ESP_LOGW(NHAN, "che_do=gia_lap cam_bien=du_lieu_gia don_vi=adc_tho");
#else
    ESP_ERROR_CHECK(nut_bat_dau(dich_vu));
    ESP_LOGW(NHAN,
        "che_do=phan_cung cam_bien=adc1 gpio=%d luu_tru=nvs don_vi=adc_tho",
        CONFIG_GAS_GPIO_MQ2);
#endif
    /* HTTP server là consumer phản hồi lệnh duy nhất; app_main không lấy mất ACK. */

    esp_err_t loi_mang = mang_bat_dau();
    if (loi_mang == ESP_OK)
        loi_mang = web_bat_dau(dich_vu);
    if (loi_mang != ESP_OK)
        ESP_LOGW(NHAN, "su_kien=web_chua_san_sang loi=%d", loi_mang);
}
