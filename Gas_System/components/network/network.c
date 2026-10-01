#include "network.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#if __has_include("wifi_local.h")
#include "wifi_local.h"
#else
#define WIFI_TEN CONFIG_GAS_WIFI_SSID
#define WIFI_MAT_KHAU CONFIG_GAS_WIFI_PASSWORD
#endif
static atomic_bool dang_ket_noi;
static bool da_bat_dau;
static esp_timer_handle_t hen;

static void thu_ket_noi(void *ctx)
{
    (void)ctx;
    if (atomic_exchange(&dang_ket_noi, true))
        return;
    esp_err_t loi = esp_wifi_connect();
    if (loi != ESP_OK) {
        atomic_store(&dang_ket_noi, false);
        ESP_LOGW("mang", "su_kien=ket_noi_that_bai loi=%d", loi);
    }
}

static void su_kien(void *ctx, esp_event_base_t base, int32_t id, void *du_lieu)
{
    (void)ctx;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&dang_ket_noi, false);
        ESP_LOGW("mang",
            "su_kien=mat_wifi reason=%u",
            (unsigned)((const wifi_event_sta_disconnected_t *)du_lieu)->reason);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = du_lieu;
        ESP_LOGI("mang", "su_kien=co_ip url=http://" IPSTR, IP2STR(&e->ip_info.ip));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        esp_err_t loi = esp_wifi_disconnect();
        if (loi != ESP_OK)
            ESP_LOGW("mang", "su_kien=loi_ngat_ket_noi loi=%d", loi);
        atomic_store(&dang_ket_noi, false);
    }
}

esp_err_t mang_bat_dau(void)
{
    if (da_bat_dau)
        return ESP_ERR_INVALID_STATE;
    size_t ten = strlen(WIFI_TEN), mat_khau = strlen(WIFI_MAT_KHAU);
    if (!ten) {
        ESP_LOGW("mang", "su_kien=chua_cau_hinh_wifi huong_dan=menuconfig_Gas_System_WiFi");
        return ESP_ERR_NOT_FOUND;
    }

    if (ten > 32 || mat_khau < 8 || mat_khau > 63) {
        ESP_LOGE("mang", "su_kien=cau_hinh_wifi_khong_hop_le");
        return ESP_ERR_INVALID_ARG;
    }

    /* Giữ một lần khởi tạo; lỗi setup được trả về, khắc phục rồi reset. */
    da_bat_dau = true;
    esp_err_t loi = esp_netif_init();
    if (loi != ESP_OK)
        return loi;
    loi = esp_event_loop_create_default();
    if (loi != ESP_OK && loi != ESP_ERR_INVALID_STATE)
        return loi;
    if (!esp_netif_create_default_wifi_sta())
        return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    loi = esp_wifi_init(&init);
    if (loi != ESP_OK)
        return loi;
    loi = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (loi != ESP_OK)
        return loi;
    wifi_config_t cfg = { 0 };
    memcpy(cfg.sta.ssid, WIFI_TEN, ten);
    memcpy(cfg.sta.password, WIFI_MAT_KHAU, mat_khau);
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    loi = esp_wifi_set_mode(WIFI_MODE_STA);
    if (loi != ESP_OK)
        return loi;
    loi = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (loi != ESP_OK)
        return loi;
    loi = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, su_kien, NULL);
    if (loi != ESP_OK)
        return loi;
    loi = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, su_kien, NULL);
    if (loi != ESP_OK)
        return loi;
    const esp_timer_create_args_t t = {
        .callback = thu_ket_noi,
        .name = "noi_lai_wifi"
    };
    loi = esp_timer_create(&t, &hen);
    if (loi != ESP_OK)
        return loi;
    atomic_init(&dang_ket_noi, false);
    loi = esp_wifi_start();
    if (loi != ESP_OK)
        return loi;
    loi = esp_timer_start_periodic(hen, 5000000);
    if (loi != ESP_OK)
        return loi;
    thu_ket_noi(NULL);
    return ESP_OK;
}
