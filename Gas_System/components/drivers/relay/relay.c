#include "relay.h"
#include "board_pins.h"
#include "driver/gpio.h"
static bool da_khoi_tao;

esp_err_t relay_khoi_tao(void)
{
    if (da_khoi_tao)
        return ESP_ERR_INVALID_STATE;

    /* Nạp LOW vào latch trước khi bật chế độ output. */
    esp_err_t loi = gpio_set_level(CONFIG_GAS_GPIO_RELAY, 0);
    if (loi != ESP_OK)
        return loi;
    const gpio_config_t c = {
        .pin_bit_mask = 1ULL << CONFIG_GAS_GPIO_RELAY,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    loi = gpio_config(&c);
    if (loi == ESP_OK)
        da_khoi_tao = true;
    return loi;
}

esp_err_t relay_dat(bool bat)
{
    if (!da_khoi_tao)
        return ESP_ERR_INVALID_STATE;
    return gpio_set_level(CONFIG_GAS_GPIO_RELAY, bat ? 1 : 0);
}
