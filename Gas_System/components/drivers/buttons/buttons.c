#include "buttons.h"

#include "board_pins.h"
#include "driver/gpio.h"

#include <stdbool.h>
#include <stddef.h>

static bool initialized;
static bool interrupts_enabled;

static const gpio_num_t pins[] = {
    CONFIG_GAS_GPIO_NUT_BAT,
    CONFIG_GAS_GPIO_NUT_TAT,
    CONFIG_GAS_GPIO_NUT_CHE_DO,
    CONFIG_GAS_GPIO_NUT_CHINH,
};

/* ISR phai ngan: khong doc GPIO, khong log, khong thay doi relay hay state he thong. */
static void button_gpio_isr(void *parameters)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR((TaskHandle_t)parameters, &higher_priority_task_woken);

    if (higher_priority_task_woken)
        portYIELD_FROM_ISR();
}

static esp_err_t ensure_gpio_isr_service(void)
{
    esp_err_t error = gpio_install_isr_service(0);

    /* ISR service la tai nguyen dung chung: button hoac D0 co the da cai no truoc. */
    return error == ESP_ERR_INVALID_STATE ? ESP_OK : error;
}

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
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    esp_err_t error = gpio_config(&config);
    if (error == ESP_OK)
        initialized = true;
    return error;
}

esp_err_t button_enable_interrupts(TaskHandle_t task_to_notify)
{
    if (!initialized || !task_to_notify)
        return ESP_ERR_INVALID_ARG;
    if (interrupts_enabled)
        return ESP_ERR_INVALID_STATE;

    esp_err_t error = ensure_gpio_isr_service();
    if (error != ESP_OK)
        return error;

    unsigned added_count = 0;
    for (; added_count < sizeof(pins) / sizeof(pins[0]); ++added_count) {
        error = gpio_isr_handler_add(pins[added_count], button_gpio_isr, task_to_notify);
        if (error != ESP_OK)
            break;
    }

    if (error != ESP_OK) {
        while (added_count > 0) {
            --added_count;
            (void)gpio_isr_handler_remove(pins[added_count]);
        }
        return error;
    }

    interrupts_enabled = true;
    return ESP_OK;
}

esp_err_t button_read(uint8_t *mask)
{
    if (mask == NULL)
        return ESP_ERR_INVALID_ARG;
    if (!initialized)
        return ESP_ERR_INVALID_STATE;

    *mask = 0;
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if (gpio_get_level(pins[i]) == 0)
            *mask |= (uint8_t)(1U << i);
    }
    return ESP_OK;
}
