#include "sensor_task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "flame_d0.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdlib.h>

/* Ham va du lieu phu chi dung trong tac vu nay. */
typedef struct {
    unsigned sample_count, index, sum;
    int sample[4];
} sensor_filter;

static bool sensor_filter_sample(sensor_filter *lcd, bool success, int raw, int *out)
{
    if (!lcd || !out)
        return false;
    if (!success || raw <= 0 || raw >= 4095) {
        *lcd = (sensor_filter){ 0 };
        return false;
    }

    if (lcd->sample_count == 4)
        lcd->sum -= (unsigned)lcd->sample[lcd->index];
    else
        ++lcd->sample_count;
    lcd->sample[lcd->index] = raw;
    lcd->sum += (unsigned)raw;
    lcd->index = (lcd->index + 1) % 4;
    *out = (int)(lcd->sum / lcd->sample_count);
    return true;
}

struct sensor_task {
    control_service_t *service;
    sensor_task_config setup;
    sensor_filter filtered;
    uint64_t sequence, previous_read_time_ms, previous_log_time_ms;
    bool has_logged, logged_sample_valid;
    bool has_logged_flame, last_flame_detected, flame_read_failed;
    TaskHandle_t task_handle;
};

static uint64_t get_time_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

/* Doc D0 doc lap voi ADC MQ-2; chi ghi log khi trang thai doi hoac co loi. */
static bool sensor_monitor_flame(sensor_task_t *worker, bool *reading_valid)
{
    if (!reading_valid)
        return false;

    *reading_valid = false;
    if (!worker->setup.monitor_flame)
        return false;

    flame_d0_reading reading;
    esp_err_t error = flame_d0_read(&reading);
    if (error != ESP_OK) {
        if (!worker->flame_read_failed)
            ESP_LOGE("cam_bien_lua", "su_kien=loi_doc_d0 gpio=%d loi=%d", CONFIG_GAS_GPIO_FIRE_D0, error);
        worker->flame_read_failed = true;
        return false;
    }

    *reading_valid = true;
    if (!worker->has_logged_flame || worker->flame_read_failed ||
        worker->last_flame_detected != reading.flame_detected) {
        ESP_LOGI("cam_bien_lua",
            "su_kien=trang_thai gpio=%d d0=%s phat_hien_lua=%d",
            CONFIG_GAS_GPIO_FIRE_D0,
            reading.d0_level ? "HIGH" : "LOW",
            reading.flame_detected);
    }

    worker->has_logged_flame = true;
    worker->flame_read_failed = false;
    worker->last_flame_detected = reading.flame_detected;
    return reading.flame_detected;
}

static esp_err_t sensor_run_once(sensor_task_t *worker)
{
    if (!worker)
        return ESP_ERR_INVALID_ARG;
    uint64_t read_time_ms = get_time_ms();
    bool fire_read_valid = false;
    bool fire_detected = sensor_monitor_flame(worker, &fire_read_valid);
    if (worker->sequence == UINT64_MAX || (worker->sequence && read_time_ms <= worker->previous_read_time_ms))
        return ESP_ERR_INVALID_STATE;
    bool sample_gap = worker->sequence && read_time_ms - worker->previous_read_time_ms >= worker->setup.sample_timeout_ms;
    int raw = 0, filtered = 0;
    esp_err_t read_error = worker->setup.read_adc(&raw, worker->setup.context);
    uint64_t finish_time_ms = get_time_ms();
    if (finish_time_ms < read_time_ms)
        return ESP_ERR_INVALID_STATE;
    bool timed_out = finish_time_ms - read_time_ms >= worker->setup.sample_timeout_ms;

    /* Giữ timestamp trước lời gọi ADC: đọc chậm không được biến thành mẫu mới.
     * Không kéo lịch sử cũ qua lỗi, đứt mẫu hoặc lần đọc quá hạn. */
    bool is_valid = sensor_filter_sample(&worker->filtered, read_error == ESP_OK && !sample_gap && !timed_out, raw, &filtered);
    control_event enable = {
        .type = CONTROL_EVENT_NEW_SAMPLE,
        .boot_id = worker->setup.boot_id,
        .sequence = ++worker->sequence,
        .sample_time_ms = read_time_ms,
        .is_valid = is_valid,
        .fire_read_valid = fire_read_valid,
        .fire_detected = fire_detected,
        .value = filtered
    };
    worker->previous_read_time_ms = read_time_ms;
    esp_err_t send_error = control_send_sample(worker->service, &enable);
    if (send_error != ESP_OK)
        worker->filtered = (sensor_filter){ 0 };
    bool sample_ok = is_valid && send_error == ESP_OK;
    if (!worker->has_logged || read_time_ms - worker->previous_log_time_ms >= 1000 || sample_ok != worker->logged_sample_valid) {
        ESP_LOGI("cam_bien",
            "su_kien=do_mau thu_tu=%" PRIu64
            " tho=%d loc=%d hop_le=%d loi_adc=%d dut_mau=%d qua_han=%d loi_gui=%d don_vi=ADC",
            worker->sequence,
            raw,
            filtered,
            is_valid,
            read_error,
            sample_gap,
            timed_out,
            send_error);
        worker->has_logged = true;
        worker->previous_log_time_ms = read_time_ms;
        worker->logged_sample_valid = sample_ok;
    }

    /* Queue đầy: bỏ mẫu, không gửi lại mẫu cũ; sequence vẫn tăng để lõi nhận ra gap. */
    return send_error;
}

static void sensor_task_loop(void *parameters)
{
    sensor_task_t *worker = parameters;
    for (;;) {
        esp_err_t error = sensor_run_once(worker);
        if (error != ESP_OK && error != ESP_ERR_TIMEOUT) {
            ESP_LOGE("cam_bien", "su_kien=loi_dich_vu loi=%d", error);
            abort();
        }

        /* Chờ sau mỗi lần đọc, không đọc dồn để bù các chu kỳ đã bỏ lỡ. */
        TickType_t delays = pdMS_TO_TICKS(worker->setup.period_ms);
        vTaskDelay(delays > 0 ? delays : 1);
    }
}

esp_err_t sensor_task_start(sensor_task_t **out, control_service_t *service, const sensor_task_config *config)
{
    if (!out || !service || !config || !config->read_adc || !config->boot_id || config->period_ms < 10 ||
        config->period_ms > 1000 || config->sample_timeout_ms <= config->period_ms || config->sample_timeout_ms > 60000)
        return ESP_ERR_INVALID_ARG;
    *out = NULL;
    control_snapshot stored_value;
    esp_err_t error = control_read_web_snapshot(service, &stored_value);
    if (error != ESP_OK)
        return error;
    if (stored_value.state.boot_id != config->boot_id ||
        stored_value.state.config.data_timeout_ms != config->sample_timeout_ms)
        return ESP_ERR_INVALID_ARG;
    sensor_task_t *worker = calloc(1, sizeof(*worker));
    if (!worker)
        return ESP_ERR_NO_MEM;
    worker->service = service;
    worker->setup = *config;
    if (xTaskCreate(sensor_task_loop, "cam_bien", 3072, worker, 4, &worker->task_handle) != pdPASS) {
        free(worker);
        return ESP_ERR_NO_MEM;
    }
    *out = worker;
    return ESP_OK;
}
