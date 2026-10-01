#include "mq2.h"
#include "board_pins.h"
#include <stddef.h>
#include "esp_log.h"

esp_err_t mq2_khoi_tao(mq2_kenh *k)
{
    if (!k)
        return ESP_ERR_INVALID_ARG;
    if (k->bo_adc || k->da_cau_hinh)
        return ESP_ERR_INVALID_STATE;
    const adc_oneshot_unit_init_cfg_t bo = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    esp_err_t loi = adc_oneshot_new_unit(&bo, &k->bo_adc);
    if (loi != ESP_OK)
        return loi;
    const adc_oneshot_chan_cfg_t c = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    loi = adc_oneshot_config_channel(k->bo_adc, (adc_channel_t)CHAN_MQ2_ADC, &c);
    if (loi != ESP_OK) {
        esp_err_t don = adc_oneshot_del_unit(k->bo_adc);
        if (don == ESP_OK)
            k->bo_adc = NULL;
        else
            ESP_LOGE("mq2", "su_kien=loi_thu_hoi_adc loi=%d loi_cau_hinh=%d", don, loi);
        return loi;
    }
    k->da_cau_hinh = true;
    return ESP_OK;
}

esp_err_t mq2_doc(const mq2_kenh *k, int *mau)
{
    if (!k || !mau)
        return ESP_ERR_INVALID_ARG;
    if (!k->da_cau_hinh || !k->bo_adc)
        return ESP_ERR_INVALID_STATE;
    int tho = 0;
    esp_err_t loi = adc_oneshot_read(k->bo_adc, (adc_channel_t)CHAN_MQ2_ADC, &tho);
    if (loi != ESP_OK)
        return loi;
    if (tho < 0 || tho > 4095)
        return ESP_ERR_INVALID_STATE;
    *mau = tho;
    return ESP_OK;
}
