#include "buttons.h"
#include "board_pins.h"
#include <stdbool.h>
#include <stddef.h>
#include "driver/gpio.h"
static bool da_khoi_tao;
static const gpio_num_t chan[] = { CONFIG_GAS_GPIO_NUT_BAT,
    CONFIG_GAS_GPIO_NUT_TAT,
    CONFIG_GAS_GPIO_NUT_CHE_DO,
    CONFIG_GAS_GPIO_NUT_CHINH };

esp_err_t nut_khoi_tao(void)
{
    if (da_khoi_tao)
        return ESP_ERR_INVALID_STATE;
    const gpio_config_t c = {
        .pin_bit_mask = (1ULL << CONFIG_GAS_GPIO_NUT_BAT) |
                                              (1ULL << CONFIG_GAS_GPIO_NUT_TAT) |
                                              (1ULL << CONFIG_GAS_GPIO_NUT_CHE_DO) |
                                              (1ULL << CONFIG_GAS_GPIO_NUT_CHINH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t loi = gpio_config(&c);
    if (loi == ESP_OK)
        da_khoi_tao = true;
    return loi;
}

esp_err_t nut_doc(uint8_t *mat_na)
{
    if (mat_na == NULL)
        return ESP_ERR_INVALID_ARG;
    if (!da_khoi_tao)
        return ESP_ERR_INVALID_STATE;
    *mat_na = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (gpio_get_level(chan[i]) == 0)
            *mat_na |= (uint8_t)(1U << i);
    return ESP_OK;
}
