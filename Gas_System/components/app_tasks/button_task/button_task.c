#include "button_task.h"
#include "button_debounce.h"

#include "buttons.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdlib.h>

static bool running;

static void button_task_loop(void *parameters)
{
    button_debouncer filter = { 0 };
    for (;;) {
        uint8_t levels;
        uint8_t edges;
        ESP_ERROR_CHECK(button_read(&levels));

        if (!button_debounce_update(&filter, levels, (uint64_t)(esp_timer_get_time() / 1000), &edges)) {
            ESP_LOGE("nut", "event=debounce_error");
            abort();
        }

        /* Nhieu canh cung luot: thu tu co dinh ON, OFF, MODE, EDIT. */
        for (unsigned i = 0; i < 4; ++i) {
            if (!(edges & (1U << i)))
                continue;

            static const char *const button_names[] = {
                "ON_INCREMENT",
                "OFF_DECREMENT",
                "AUTO_MANUAL",
                "EDIT_SAVE",
            };
            ESP_LOGI("nut", "event=stable_press button=%s", button_names[i]);
            esp_err_t error = control_send_button(parameters, (button_action)i);
            if (error != ESP_OK)
                ESP_LOGW("nut", "event=send_failed button=%u error=%d", i, error);
        }

        /* Het 10 ms thi quet de chong rung; GPIO interrupt thi danh thuc task som hon. */
        TickType_t wait_ticks = pdMS_TO_TICKS(10);
        (void)ulTaskNotifyTake(pdTRUE, wait_ticks > 0 ? wait_ticks : 1);
    }
}

esp_err_t button_task_start(control_service_t *service)
{
    if (!service)
        return ESP_ERR_INVALID_ARG;
    if (running)
        return ESP_ERR_INVALID_STATE;

    uint8_t levels;
    esp_err_t error = button_read(&levels);
    if (error != ESP_OK)
        return error;

    TaskHandle_t task_handle = NULL;
    if (xTaskCreate(button_task_loop, "doc_nut", 3072, service, 4, &task_handle) != pdPASS)
        return ESP_ERR_NO_MEM;

    error = button_enable_interrupts(task_handle);
    if (error != ESP_OK) {
        vTaskDelete(task_handle);
        return error;
    }

    running = true;
    return ESP_OK;
}
