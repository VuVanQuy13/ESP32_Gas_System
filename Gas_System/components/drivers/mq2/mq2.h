#pragma once
#include <stdbool.h>
#include "esp_adc/adc_oneshot.h"

typedef struct {
    adc_oneshot_unit_handle_t bo_adc;
    bool da_cau_hinh;
} mq2_kenh;

/* Struct khởi tạo bằng 0. Startup cấu hình một lần, sau đó chỉ sensor_task đọc.
 * AO qua chia áp vào CONFIG_GAS_GPIO_MQ2 (mặc định GPIO34/ADC1_CH6).
 * Kênh ADC1 tự chọn theo GPIO; DO không sử dụng. */
esp_err_t mq2_khoi_tao(mq2_kenh *kenh);

/* Chỉ sửa đầu ra khi thành công; trả mã ADC 12 bit, chưa chuyển mV/ppm. */
esp_err_t mq2_doc(const mq2_kenh *kenh, int *mau);
