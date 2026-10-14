#include "relay.h"
#include "board_pins.h"
#include "driver/gpio.h"
static bool initialized;

esp_err_t relay_initialize(void)
{
    if (initialized)
        return ESP_ERR_INVALID_STATE;

    /* Nạp LOW vào latch trước khi bật chế độ output. */
    esp_err_t error = gpio_set_level(CONFIG_GAS_GPIO_RELAY, 0);
    if (error != ESP_OK)
        return error;
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << CONFIG_GAS_GPIO_RELAY,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    error = gpio_config(&config);
    if (error == ESP_OK)
        initialized = true;
    return error;
}

esp_err_t relay_set(bool on)
{
    if (!initialized)
        return ESP_ERR_INVALID_STATE;
    return gpio_set_level(CONFIG_GAS_GPIO_RELAY, on ? 1 : 0);
}
