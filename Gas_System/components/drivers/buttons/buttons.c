#include "buttons.h"
#include "board_pins.h"
#include <stdbool.h>
#include <stddef.h>
#include "driver/gpio.h"
static bool initialized;
static const gpio_num_t pins[] = { 
    CONFIG_GAS_GPIO_NUT_BAT,
    CONFIG_GAS_GPIO_NUT_TAT,
    CONFIG_GAS_GPIO_NUT_CHE_DO,
    CONFIG_GAS_GPIO_NUT_CHINH };

esp_err_t button_initialize(void)
{
    if (initialized)
        return ESP_ERR_INVALID_STATE;
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << CONFIG_GAS_GPIO_NUT_BAT) |
                                              (1ULL << CONFIG_GAS_GPIO_NUT_TAT) |
                                              (1ULL << CONFIG_GAS_GPIO_NUT_CHE_DO) |
                                              (1ULL << CONFIG_GAS_GPIO_NUT_CHINH),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t error = gpio_config(&config);
    if (error == ESP_OK)
        initialized = true;
    return error;
}

esp_err_t button_read(uint8_t *mask)
{
    if (mask == NULL)
        return ESP_ERR_INVALID_ARG;
    if (!initialized)
        return ESP_ERR_INVALID_STATE;
    *mask = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (gpio_get_level(pins[i]) == 0)
            *mask |= (uint8_t)(1U << i);
    return ESP_OK;
}
