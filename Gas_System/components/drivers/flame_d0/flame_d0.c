#include "flame_d0.h"

#include "driver/gpio.h"
#include "flame_d0_logic.h"
#include "sdkconfig.h"

#include <stdbool.h>

static bool initialized;
static bool interrupts_enabled;

/* ISR phai ngan: chi danh thuc sensor_task, khong doc D0 hay thao tac relay. */
static void flame_d0_gpio_isr(void *parameters)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR((TaskHandle_t)parameters, &higher_priority_task_woken);

    if (higher_priority_task_woken)
        portYIELD_FROM_ISR();
}

static esp_err_t ensure_gpio_isr_service(void)
{
    esp_err_t error = gpio_install_isr_service(0);

    return error == ESP_ERR_INVALID_STATE ? ESP_OK : error;
}

esp_err_t flame_d0_initialize(void)
{
    if (initialized)
        return ESP_ERR_INVALID_STATE;

    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << CONFIG_GAS_GPIO_FIRE_D0,
        .mode = GPIO_MODE_INPUT,
        /* Khong bat pull noi: ho tro ca chan input-only GPIO34..39. */
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        /* Can bat ca canh phat hien lua va canh het lua. */
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    esp_err_t error = gpio_config(&config);
    if (error == ESP_OK)
        initialized = true;
    return error;
}

esp_err_t flame_d0_enable_interrupts(TaskHandle_t task_to_notify)
{
    if (!initialized || !task_to_notify)
        return ESP_ERR_INVALID_ARG;
    if (interrupts_enabled)
        return ESP_ERR_INVALID_STATE;

    esp_err_t error = ensure_gpio_isr_service();
    if (error != ESP_OK)
        return error;

    error = gpio_isr_handler_add(CONFIG_GAS_GPIO_FIRE_D0, flame_d0_gpio_isr, task_to_notify);
    if (error != ESP_OK)
        return error;

    interrupts_enabled = true;
    return ESP_OK;
}

esp_err_t flame_d0_read(flame_d0_reading *reading)
{
    if (!reading)
        return ESP_ERR_INVALID_ARG;
    if (!initialized)
        return ESP_ERR_INVALID_STATE;

    int level = gpio_get_level(CONFIG_GAS_GPIO_FIRE_D0);
    if (level != 0 && level != 1)
        return ESP_FAIL;

    reading->d0_level = level;
    reading->flame_detected = flame_d0_level_detects_fire(level, CONFIG_GAS_FIRE_D0_ACTIVE_LOW);
    return ESP_OK;
}
