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
#include "esp_random.h"
#include "network.h"
#include "web_service.h"
#include "telegram_notify.h"

static const char *TAG = "ung_dung";

/* Đối tượng giữ handle ADC1; phải sống suốt thời gian sensor task chạy. */
static mq2_channel mq2_adc_channel;
static lcd_i2c lcd_display;

static esp_err_t read_real_adc(int *sample, void *context)
{
    return mq2_read(context, sample);
}

static esp_err_t set_real_relay(bool on, void *context)
{
    (void)context;
    return relay_set(on);
}

/* Điểm vào ESP-IDF: tạo tài nguyên dùng chung rồi khởi chạy từng FreeRTOS task. */
void app_main(void)
{
    /* Ngưỡng dùng ADC thô. MQ-2 chỉ sẵn sàng sau thời gian cấu hình và các mẫu hợp lệ. */
    control_task_config setup = {
        .policy = {
            .warmup_time_ms = CONFIG_MQ2_ON_DINH_GIAY > 0
                                       ? (uint64_t)CONFIG_MQ2_ON_DINH_GIAY * 1000
                                       : UINT64_MAX,
            .data_timeout_ms = 500,
            .edit_timeout_ms = 30000,
            .save_timeout_ms = 2000,
            .required_sample_count = 3,
            .minimum_threshold = 100,
            .maximum_threshold = 4000,
            .threshold_hysteresis = 50,
            .initial_draft_threshold = 600
        },
        .boot_id = 1,
        .saved_data_valid = false,
        .set_relay = set_real_relay,
    };

    /* Nhận dạng boot 64-bit, không phải khóa xác thực. Tạo trước khi ADC hoạt động. */
    setup.boot_id = ((uint64_t)esp_random() << 32) | esp_random();
    if (!setup.boot_id)
        setup.boot_id = 1;
    /* 0 nghĩa chưa xác định qua tài liệu/thực nghiệm: chưa cho Ready.
     * Khi đó Auto vẫn giữ relay tắt. */

    ESP_LOGI(TAG,
        "su_kien=cau_hinh_gpio mq2=%d relay=%d sda=%d scl=%d nut=%d,%d,%d,%d",
        CONFIG_GAS_GPIO_MQ2,
        CONFIG_GAS_GPIO_RELAY,
        CONFIG_GAS_GPIO_LCD_SDA,
        CONFIG_GAS_GPIO_LCD_SCL,
        CONFIG_GAS_GPIO_NUT_BAT,
        CONFIG_GAS_GPIO_NUT_TAT,
        CONFIG_GAS_GPIO_NUT_CHE_DO,
        CONFIG_GAS_GPIO_NUT_CHINH
    );

    ESP_ERROR_CHECK(relay_initialize());
    ESP_ERROR_CHECK(button_initialize());
    esp_err_t adc_error = mq2_initialize(&mq2_adc_channel);
    if (adc_error != ESP_OK)
        ESP_LOGE(TAG, "su_kien=loi_khoi_tao_adc loi=%d", adc_error);
    if (CONFIG_MQ2_ON_DINH_GIAY == 0)
        ESP_LOGW(TAG, "su_kien=chua_cau_hinh_on_dinh_mq2 san_sang=bi_khoa");

    storage_store store = { 0 };
    esp_err_t storage_error =
        storage_initialize(&store, setup.policy.minimum_threshold, setup.policy.maximum_threshold);

    if (storage_error == ESP_OK)
        storage_error = storage_read(&store, &setup.saved_threshold);
    setup.saved_data_valid = storage_error == ESP_OK;
    if (storage_error != ESP_OK)
        ESP_LOGW(TAG, "su_kien=can_cau_hinh loi_nvs=%d", storage_error);
    else
        ESP_LOGI(TAG, "su_kien=khoi_phuc_nguong nguong=%d", setup.saved_threshold);

#if CONFIG_GAS_TELEGRAM_ENABLED
    esp_err_t telegram_error = telegram_notify_start();
    setup.telegram_notifications_ready = telegram_error == ESP_OK;
    if (telegram_error == ESP_OK)
        ESP_LOGI(TAG, "su_kien=telegram_worker ket_qua=san_sang");
    else
        ESP_LOGW(TAG, "su_kien=telegram_worker ket_qua=khong_san_sang loi=%d", telegram_error);
#else
    setup.telegram_notifications_ready = false;
    ESP_LOGI(TAG, "su_kien=telegram_worker ket_qua=tat_trong_menuconfig");
#endif

    control_service_t *service = NULL;
    ESP_ERROR_CHECK(control_task_start(&service, &setup));

    storage_task_t *storage_worker = NULL;
    ESP_ERROR_CHECK(storage_task_start(&storage_worker, service, &store));

    const sensor_task_config sensor_setup = {
        .boot_id = setup.boot_id,
        .period_ms = 100,
        .sample_timeout_ms = 500,
        .read_adc = read_real_adc,
        .context = &mq2_adc_channel,
    };
    sensor_task_t *sensor_worker = NULL;
    ESP_ERROR_CHECK(sensor_task_start(&sensor_worker, service, &sensor_setup));

    lcd_display.address = CONFIG_LCD_DIA_CHI;
#if CONFIG_LCD_TU_TIM
    lcd_display.auto_find_address = true;
#endif

    lcd_display.mapping = lcd_default_mapping();
#if CONFIG_LCD_DEN_MUC_CAO
    lcd_display.mapping.backlight_active_high = true;
#else
    lcd_display.mapping.backlight_active_high = false;
#endif
    display_task_config display_setup = {
        .output = lcd_i2c_output,
        .context = &lcd_display
    };

    display_task_t *display_worker = NULL;
    ESP_ERROR_CHECK(display_task_start(&display_worker, service, &display_setup));

    ESP_ERROR_CHECK(button_task_start(service));
    ESP_LOGW(TAG,
        "che_do=phan_cung cam_bien=adc1 gpio=%d luu_tru=nvs don_vi=adc_tho",
        CONFIG_GAS_GPIO_MQ2);
    /* HTTP server là consumer phản hồi lệnh duy nhất; app_main không lấy mất ACK. */

    esp_err_t network_error = network_start();
    if (network_error == ESP_OK)
        network_error = web_start(service);
    if (network_error != ESP_OK)
        ESP_LOGW(TAG, "su_kien=web_chua_san_sang loi=%d", network_error);
}
