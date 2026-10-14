#include "button_task.h"
#include "buttons.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

/* Ham va du lieu phu chi dung trong tac vu nay. */
typedef struct {
    bool initialize;
    uint8_t candidate, stable, armed;
    uint64_t change_time_ms[4], previous_time_ms;
} button_debouncer;

static bool button_debounce(button_debouncer *lcd, uint8_t m, uint64_t ms, uint8_t *out)
{
    if (!lcd || !out || m > 15 || (lcd->initialize && ms < lcd->previous_time_ms))
        return false;
    *out = 0;
    if (!lcd->initialize) {
        lcd->initialize = true;
        lcd->candidate = m;
        lcd->stable = m;
        lcd->armed = (uint8_t)(~m & 15);
        for (unsigned i = 0; i < 4; ++i)
            lcd->change_time_ms[i] = ms;
    }
    lcd->previous_time_ms = ms;
    for (unsigned i = 0; i < 4; ++i) {
        uint8_t bits = (uint8_t)(1U << i);
        if ((m ^ lcd->candidate) & bits) {
            lcd->candidate ^= bits;
            lcd->change_time_ms[i] = ms;
        }

        if (((lcd->candidate ^ lcd->stable) & bits) && ms - lcd->change_time_ms[i] >= 30) {
            lcd->stable ^= bits;
            if (!(lcd->stable & bits))
                lcd->armed |= bits;
            else {
                if (lcd->armed & bits)
                    *out |= bits;
                lcd->armed &= (uint8_t)~bits;
            }
        }
    }

    return true;
}

static bool running;

static void button_task_loop(void *parameters)
{
    button_debouncer filtered = { 0 };
    for (;;) {
        uint8_t levels, edges;
        ESP_ERROR_CHECK(button_read(&levels));
        if (!button_debounce(&filtered, levels, (uint64_t)(esp_timer_get_time() / 1000), &edges)) {
            ESP_LOGE("nut", "su_kien=loi_bo_loc");
            abort();
        }

        /* Nhiều cạnh cùng lượt: thứ tự cố định ON, OFF, MODE, EDIT. */
        for (unsigned i = 0; i < 4; ++i)
            if (edges & (1U << i)) {
                static const char *const button_names[] = {
                    "BAT_TANG", "TAT_GIAM", "AUTO_MANUAL", "CHINH_LUU"
                };
                ESP_LOGI("nut", "su_kien=nhan_on_dinh nut=%s", button_names[i]);
                esp_err_t error = control_send_button(parameters, (button_action)i);
                if (error != ESP_OK)
                    ESP_LOGW("nut", "su_kien=gui_that_bai nut=%u loi=%d", i, error);
            }
        vTaskDelay(pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1);
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
    if (xTaskCreate(button_task_loop, "doc_nut", 3072, service, 4, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    running = true;
    return ESP_OK;
}
