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
static atomic_bool connecting;
static bool started;
static esp_timer_handle_t retry_timer;

static void network_try_connect(void *ctx)
{
    (void)ctx;
    if (atomic_exchange(&connecting, true))
        return;
    esp_err_t error = esp_wifi_connect();
    if (error != ESP_OK) {
        atomic_store(&connecting, false);
        ESP_LOGW("mang", "su_kien=ket_noi_that_bai loi=%d", error);
    }
}

static void event(void *ctx, esp_event_base_t base, int32_t id, void *data)
{
    (void)ctx;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&connecting, false);
        ESP_LOGW("mang",
            "su_kien=mat_wifi reason=%u",
            (unsigned)((const wifi_event_sta_disconnected_t *)data)->reason);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *enable = data;
        ESP_LOGI("mang", "su_kien=co_ip url=http://" IPSTR, IP2STR(&enable->ip_info.ip));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        esp_err_t error = esp_wifi_disconnect();
        if (error != ESP_OK)
            ESP_LOGW("mang", "su_kien=loi_ngat_ket_noi loi=%d", error);
        atomic_store(&connecting, false);
    }
}

esp_err_t network_start(void)
{
    if (started)
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
    started = true;
    esp_err_t error = esp_netif_init();
    if (error != ESP_OK)
        return error;
    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return error;
    if (!esp_netif_create_default_wifi_sta())
        return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    error = esp_wifi_init(&init);
    if (error != ESP_OK)
        return error;
    error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error != ESP_OK)
        return error;
    wifi_config_t cfg = { 0 };
    memcpy(cfg.sta.ssid, WIFI_TEN, ten);
    memcpy(cfg.sta.password, WIFI_MAT_KHAU, mat_khau);
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK)
        return error;
    error = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (error != ESP_OK)
        return error;
    error = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL);
    if (error != ESP_OK)
        return error;
    error = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event, NULL);
    if (error != ESP_OK)
        return error;
    const esp_timer_create_args_t worker = {
        .callback = network_try_connect,
        .name = "noi_lai_wifi"
    };
    error = esp_timer_create(&worker, &retry_timer);
    if (error != ESP_OK)
        return error;
    atomic_init(&connecting, false);
    error = esp_wifi_start();
    if (error != ESP_OK)
        return error;
    error = esp_timer_start_periodic(retry_timer, 5000000);
    if (error != ESP_OK)
        return error;
    network_try_connect(NULL);
    return ESP_OK;
}
