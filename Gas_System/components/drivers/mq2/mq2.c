#include "mq2.h"
#include "board_pins.h"
#include <stddef.h>
#include "esp_log.h"

esp_err_t mq2_initialize(mq2_channel *result)
{
    if (!result)
        return ESP_ERR_INVALID_ARG;
    if (result->adc_unit || result->configured)
        return ESP_ERR_INVALID_STATE;

    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    esp_err_t error = adc_oneshot_new_unit(&unit_config, &result->adc_unit);
    if (error != ESP_OK)
        return error;
    const adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    error = adc_oneshot_config_channel(result->adc_unit, (adc_channel_t)MQ2_ADC_CHANNEL, &config);
    if (error != ESP_OK) {
        esp_err_t cleanup_error = adc_oneshot_del_unit(result->adc_unit);
        if (cleanup_error == ESP_OK)
            result->adc_unit = NULL;
        else
            ESP_LOGE("mq2", "su_kien=loi_thu_hoi_adc loi=%d loi_cau_hinh=%d", cleanup_error, error);
        return error;
    }
    result->configured = true;
    return ESP_OK;
}

esp_err_t mq2_read(const mq2_channel *result, int *sample)
{
    // Kiểm tra đối số và xác nhận driver ADC đã được khởi tạo.
    if (!result || !sample)
        return ESP_ERR_INVALID_ARG;
    if (!result->configured || !result->adc_unit)
        return ESP_ERR_INVALID_STATE;

    
    int raw = 0;
    esp_err_t error = adc_oneshot_read(result->adc_unit, (adc_channel_t)MQ2_ADC_CHANNEL, &raw);
    if (error != ESP_OK)
        return error;
    if (raw < 0 || raw > 4095)
        return ESP_ERR_INVALID_STATE;
    *sample = raw;
    
    return ESP_OK;
}
