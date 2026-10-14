#include "display_task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Ham va du lieu phu chi dung trong tac vu nay. */
static void close_count(char out[17], const char *text)
{
    size_t length = strlen(text);
    if (length > 16)
        length = 16;
    memset(out, ' ', 16);
    memcpy(out, text, length);
    out[16] = '\0';
}

static esp_err_t display_build_frame(const control_snapshot *snapshot, uint64_t now, lcd_frame *result)
{
    if (!snapshot || !result || !snapshot->state.initialized)
        return ESP_ERR_INVALID_ARG;
    const control_state *state = &snapshot->state;
    char trai[24], duoi[32], tren[32];
    bool cu = now < state->current_time_ms || now - state->current_time_ms >= 1000;
    bool previous_sample = state->has_sample && (now < state->sample_time_ms ||
                                      now - state->sample_time_ms >= state->config.data_timeout_ms);
    if (cu)
        snprintf(trai, sizeof(trai), "MAT DONGBO");
    else if (state->sensor_status == SENSOR_ERROR || previous_sample)
        snprintf(trai, sizeof(trai), "LOI CB");
    else if (state->sensor_status != SENSOR_READY)
        snprintf(trai, sizeof(trai), "ON DINH");
    else if (!state->has_sample || state->sample < 0 || state->sample > 4095)
        snprintf(trai, sizeof(trai), "LOI CB");
    else
        snprintf(trai,
            sizeof(trai),
            state->configuration_valid && state->sample >= state->threshold ? "VUOT %4d" : "ADC:%4d",
            state->sample);
    snprintf(tren,
        sizeof(tren),
        "%c R:%c %s",
        state->mode == CONTROL_MODE_AUTO ? 'A' : 'M',
        cu || snapshot->output_error != ESP_OK ? '?' : (state->relay_on ? '1' : '0'),
        trai);
    if (cu)
        snprintf(duoi, sizeof(duoi), "CHO DU LIEU MOI");
    else if (state->edit_status == EDIT_SAVING)
        snprintf(duoi,
            sizeof(duoi),
            state->save_result_unknown ? "LUU CHUA RO" : "DANG LUU:%4d",
            state->draft);
    else if (state->edit_status == EDIT_ACTIVE)
        snprintf(duoi, sizeof(duoi), snapshot->storage_error ? "LOI LUU:%4d" : "CHINH:%4d", state->draft);
    else if (!state->configuration_valid)
        snprintf(duoi, sizeof(duoi), "CAN DAT NGUONG");
    else
        snprintf(duoi, sizeof(duoi), "NGUONG:%4d", state->threshold);
    close_count(result->close_count[0], tren);
    close_count(result->close_count[1], duoi);
    return ESP_OK;
}

struct display_task {
    control_service_t *service;
    display_task_config config;
    lcd_frame cu;
    bool has_output, has_error;
    uint64_t last_retry_time_ms;
    TaskHandle_t task_handle;
};

static esp_err_t display_run_once(display_task_t *worker)
{
    if (!worker)
        return ESP_ERR_INVALID_ARG;
    uint64_t now = (uint64_t)(esp_timer_get_time() / 1000);

    /* Lỗi I2C: giãn retry một giây, mỗi lần lấy lại snapshot mới nhất. */
    if (worker->has_error && now >= worker->last_retry_time_ms && now - worker->last_retry_time_ms < 1000)
        return ESP_ERR_TIMEOUT;
    control_snapshot snapshot;
    esp_err_t error = control_read_lcd_snapshot(worker->service, &snapshot);
    if (error != ESP_OK)
        return error;
    lcd_frame moi;
    error = display_build_frame(&snapshot, now, &moi);
    if (error != ESP_OK)
        return error;
    if (worker->has_output && !worker->has_error && memcmp(&moi, &worker->cu, sizeof(moi)) == 0)
        return ESP_OK;
    worker->last_retry_time_ms = now;
    error = worker->config.output(&moi, worker->config.context);
    if (error != ESP_OK) {
        worker->has_error = true;
        ESP_LOGE("hien_thi", "su_kien=loi_lcd loi=%d", error);
        return error;
    }

    if (worker->has_error)
        ESP_LOGI("hien_thi", "su_kien=lcd_phuc_hoi");
    worker->has_error = false;
    worker->has_output = true;
    worker->cu = moi;
    return ESP_OK;
}

static void display_task_loop(void *ctx)
{
    for (;;) {
        esp_err_t error = display_run_once(ctx);
        if (error != ESP_OK && error != ESP_ERR_TIMEOUT)
            ESP_LOGW("hien_thi", "su_kien=cap_nhat_that_bai loi=%d", error);
        vTaskDelay(pdMS_TO_TICKS(200) > 0 ? pdMS_TO_TICKS(200) : 1);
    }
}

esp_err_t display_task_start(display_task_t **out, control_service_t *service, const display_task_config *config)
{
    if (!out || !service || !config || !config->output)
        return ESP_ERR_INVALID_ARG;
    *out = NULL;
    display_task_t *worker = calloc(1, sizeof(*worker));
    if (!worker)
        return ESP_ERR_NO_MEM;
    worker->service = service;
    worker->config = *config;
    if (xTaskCreate(display_task_loop, "hien_thi", 3072, worker, 2, &worker->task_handle) != pdPASS) {
        free(worker);
        return ESP_ERR_NO_MEM;
    }
    *out = worker;
    return ESP_OK;
}
