#include "storage_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdlib.h>

struct storage_task {
    control_service_t *service;
    storage_store store;
    bool has_result;
    control_event result_event;
    TaskHandle_t task;
};

static esp_err_t storage_run_once(storage_task_t *worker)
{
    if (!worker)
        return ESP_ERR_INVALID_ARG;
    if (!worker->has_result) {
        storage_job job;
        esp_err_t error = control_take_storage_job(worker->service, &job);
        if (error != ESP_OK)
            return error;
        error = storage_write(&worker->store, job.threshold);
        ESP_LOGI("luu_tru",
            "su_kien=ghi_xong boot=%" PRIu64 " lan=%" PRIu64 " nguong=%d loi=%d",
            job.boot_id,
            job.save_id,
            job.threshold,
            error);
        worker->result_event = (control_event){
            .type = error == ESP_OK ? CONTROL_EVENT_SAVE_OK : CONTROL_EVENT_SAVE_FAILED,
            .boot_id = job.boot_id,
            .save_id = job.save_id
        };
        worker->has_result = true;
    }
    esp_err_t error = control_report_storage_result(worker->service, &worker->result_event);
    if (error == ESP_OK)
        worker->has_result = false;

    /* Queue đầy: giữ nguyên kết quả, lượt sau chỉ gửi lại, không ghi NVS lại. */
    return error;
}

static void storage_task_loop(void *parameters)
{
    for (;;) {
        esp_err_t error = storage_run_once(parameters);
        if (error != ESP_OK && error != ESP_ERR_NOT_FOUND && error != ESP_ERR_TIMEOUT) {
            ESP_LOGE("luu_tru", "su_kien=loi_dich_vu loi=%d", error);
            abort();
        }
        vTaskDelay(pdMS_TO_TICKS(20) > 0 ? pdMS_TO_TICKS(20) : 1);
    }
}

esp_err_t storage_task_start(storage_task_t **out, control_service_t *service, const storage_store *result)
{
    if (!out || !service || !result || !result->configured)
        return ESP_ERR_INVALID_ARG;
    *out = NULL;
    storage_task_t *worker = calloc(1, sizeof(*worker));
    if (!worker)
        return ESP_ERR_NO_MEM;
    worker->service = service;
    worker->store = *result;
    if (xTaskCreate(storage_task_loop, "luu_tru", 3072, worker, 3, &worker->task) != pdPASS) {
        free(worker);
        return ESP_ERR_NO_MEM;
    }
    *out = worker;
    return ESP_OK;
}
