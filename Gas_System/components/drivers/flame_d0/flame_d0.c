#include "flame_d0.h"

#include "driver/gpio.h"
#include "flame_d0_logic.h"
#include "sdkconfig.h"

esp_err_t flame_d0_initialize(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << CONFIG_GAS_GPIO_FIRE_D0,
        .mode = GPIO_MODE_INPUT,
        /* Khong bat pull noi: ho tro ca chan input-only GPIO34..39. */
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&config);
}

esp_err_t flame_d0_read(flame_d0_reading *reading)
{
    if (!reading)
        return ESP_ERR_INVALID_ARG;

    int level = gpio_get_level(CONFIG_GAS_GPIO_FIRE_D0);
    if (level != 0 && level != 1)
        return ESP_FAIL;

    reading->d0_level = level;
    reading->flame_detected = flame_d0_level_detects_fire(level, CONFIG_GAS_FIRE_D0_ACTIVE_LOW);
    return ESP_OK;
}
