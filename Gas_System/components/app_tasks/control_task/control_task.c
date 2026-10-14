#include "control_task.h"

#include <inttypes.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define COMMAND_QUEUE_LENGTH 16
#define NGAN_XEP_BYTE 4096
#define UU_TIEN 5
#define CHO_TOI_DA_MS 100

static const char *TAG = "dieu_khien";

static void check_internal(BaseType_t result_event)
{
    if (result_event != pdPASS) {
        ESP_LOGE(TAG, "su_kien=vi_pham_bat_bien_hang_doi");
        abort();
    }
}

typedef struct {
    uint64_t command_id;
    control_event event;
    bool is_button;
    button_action button;
    bool from_web;
    uint64_t received_time_ms, control_revision;
} control_packet;

struct control_service {
    control_state state;
    control_task_config setup;
    QueueHandle_t input_queue, response, lcd, web, storage_jobs, storage_results;
    SemaphoreHandle_t response_slots;
    TaskHandle_t task_handle;
    uint64_t revision;
    uint64_t control_revision;
    esp_err_t output_error;
    bool storage_error;
};

static uint64_t get_time_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void phat_ban_chup(control_service_t *service)
{
    control_snapshot stored_value = {
        .revision = ++service->revision,
        .state = service->state,
        .output_error = service->output_error,
        .storage_error = service->storage_error,
        .control_revision = service->control_revision
    };

    /* Queue có đúng một phần tử: luôn giữ bản mới nhất. */
    check_internal(xQueueOverwrite(service->lcd, &stored_value));
    check_internal(xQueueOverwrite(service->web, &stored_value));
}

/* -1: chua du dieu kien so sanh, 0: duoi nguong, 1: cham/vuot nguong. */
static int get_alarm_level(const control_state *state)
{
    if (!state->configuration_valid || !state->has_sample || state->sensor_status != SENSOR_READY)
        return -1;
    return state->sample >= state->threshold;
}

static control_result process(control_service_t *service, const control_event *event, uint64_t command_id)
{
    control_result result;
    int previous_alarm = get_alarm_level(&service->state);
    control_mode previous_mode = service->state.mode;
    bool previous_relay = service->state.relay_on;
    sensor_status previous_sensor = service->state.sensor_status;
    edit_status previous_edit = service->state.edit_status;
    bool previous_save_unknown = service->state.save_result_unknown;
    control_result_code code = control_process_event(&service->state, event, get_time_ms(), &result);

    /* Timeout có thể đổi đầu ra kể cả khi lệnh bị từ chối. */
    if (result.relay_changed || service->output_error != ESP_OK) {
        service->output_error = service->setup.set_relay(result.relay_on, service->setup.context);
        if (service->output_error != ESP_OK)
            ESP_LOGE(TAG, "su_kien=loi_gpio ma=%d muc=%d", service->output_error, result.relay_on);
    }

    if (result.relay_changed && service->output_error == ESP_OK)
        ESP_LOGI(TAG,
            "su_kien=xuat_relay yeu_cau=%s che_do=%s",
            result.relay_on ? "BAT" : "TAT",
            result.mode == CONTROL_MODE_AUTO ? "AUTO" : "MANUAL");
    if (result.needs_save) {
        storage_job job = { service->state.boot_id, result.save_id, result.threshold_to_save };
        if (xQueueSend(service->storage_jobs, &job, 0) != pdPASS) {
            /* Chưa giao storage nên biết chắc yêu cầu mới chưa được ghi. */
            ESP_LOGE(TAG, "su_kien=hang_luu_day id=%" PRIu64, result.save_id);
            control_event error = {
                .type = CONTROL_EVENT_SAVE_FAILED,
                .boot_id = service->state.boot_id,
                .save_id = result.save_id
            };
            (void)control_process_event(&service->state, &error, get_time_ms(), &result);
            result.event = event->type;
            code = result.result_code;
        }
    }

    if (previous_mode != service->state.mode || previous_relay != service->state.relay_on) {
        if (service->control_revision == UINT64_MAX) {
            ESP_LOGE(TAG, "su_kien=het_phien_dieu_khien");
            abort();
        }
        ++service->control_revision;
    }

    if (code == CONTROL_STORAGE_ERROR)
        service->storage_error = true;
    else if (code == CONTROL_OK &&
             (event->type == CONTROL_EVENT_BEGIN_EDIT || event->type == CONTROL_EVENT_SAVE_DRAFT ||
                 event->type == CONTROL_EVENT_SAVE_OK))
        service->storage_error = false;
    int current_alarm = get_alarm_level(&service->state);
    if (previous_alarm != current_alarm)
        ESP_LOGI(TAG,
            "su_kien=canh_bao trang_thai=%s mau=%d nguong=%d",
            current_alarm < 0 ? "CHUA_DU_DIEU_KIEN"
            : current_alarm   ? "VUOT_NGUONG"
                             : "DUOI_NGUONG",
            service->state.sample,
            service->state.threshold);
    if ((event->type != CONTROL_EVENT_CHECK_TIMEOUTS && event->type != CONTROL_EVENT_NEW_SAMPLE) ||
        result.relay_changed || previous_sensor != result.sensor_status ||
        previous_edit != result.edit_status || previous_save_unknown != result.save_result_unknown) {
        ESP_LOGI(TAG,
            "lenh=%" PRIu64
            " su_kien=%d ket_qua=%d che_do=%s cam_bien=%s relay=%s loi_dau_ra=%d chinh=%s "
            "luu_chua_ro=%d nguong=%d ban_nhap=%d cau_hinh_hop_le=%d",
            command_id,
            event->type,
            code,
            result.mode == CONTROL_MODE_AUTO ? "AUTO" : "MANUAL",
            result.sensor_status == SENSOR_READY       ? "SAN_SANG"
            : result.sensor_status == SENSOR_ERROR ? "LOI"
                                                       : "DANG_ON_DINH",
            result.relay_on ? "BAT" : "TAT",
            service->output_error,
            result.edit_status == EDIT_IDLE  ? "KHONG_CHINH"
            : result.edit_status == EDIT_ACTIVE ? "DANG_CHINH"
                                                  : "DANG_LUU",
            result.save_result_unknown,
            service->state.threshold,
            service->state.draft,
            service->state.configuration_valid);
    }

    return result;
}

static control_event translate_button(const control_state *state, button_action button)
{
    control_event enable = {
        .boot_id = state->boot_id,
        .edit_session_id = state->edit_session_id,
        .expected_revision = state->draft_revision
    };
    if (button == BUTTON_ACTION_TOGGLE_MODE) {
        enable.type = CONTROL_EVENT_SET_MODE;
        enable.value = state->mode == CONTROL_MODE_AUTO ? CONTROL_MODE_MANUAL : CONTROL_MODE_AUTO;
    } else if (button == BUTTON_ACTION_EDIT_SAVE) {
        enable.type = state->edit_status == EDIT_IDLE ? CONTROL_EVENT_BEGIN_EDIT : CONTROL_EVENT_SAVE_DRAFT;
    } else if (state->edit_status != EDIT_IDLE) {
        enable.type = CONTROL_EVENT_SET_DRAFT;

        /* Bước 10 đơn vị thử nghiệm; chặn ở biên, tránh tràn số int. */
        int64_t moi = (int64_t)state->draft + (button == BUTTON_ACTION_ON_INCREMENT ? 10 : -10);
        if (moi > state->config.maximum_threshold)
            moi = state->config.maximum_threshold;
        if (moi < state->config.minimum_threshold)
            moi = state->config.minimum_threshold;
        enable.value = (int)moi;
    } else {
        enable.type = CONTROL_EVENT_SET_RELAY;
        enable.value = button == BUTTON_ACTION_ON_INCREMENT;
    }

    return enable;
}

esp_err_t control_send_button(control_service_t *service, button_action button)
{
    if (!service || button < BUTTON_ACTION_ON_INCREMENT || button > BUTTON_ACTION_EDIT_SAVE)
        return ESP_ERR_INVALID_ARG;
    control_packet packet = {
        .is_button = true,
        .button = button
    };
    if (xQueueSend(service->input_queue, &packet, 0) != pdPASS) {
        ESP_LOGW(TAG, "su_kien=bo_nut_hang_day nut=%d", button);
        return ESP_ERR_TIMEOUT;
    }
    xTaskNotifyGive(service->task_handle);
    return ESP_OK;
}

static bool control_run_once(control_service_t *service)
{
    if (service == NULL)
        return false;

    /* Mỗi lượt hữu hạn: kiểm tra thời gian, một kết quả lưu, một đầu vào.
     * Flood lệnh không được làm mất bước kiểm tra timeout. */
    control_event tick = {
        .type = CONTROL_EVENT_CHECK_TIMEOUTS,
        .boot_id = service->state.boot_id
    };
    (void)process(service, &tick, 0);
    bool co_viec = false;
    control_event storage;
    if (xQueueReceive(service->storage_results, &storage, 0) == pdPASS) {
        (void)process(service, &storage, 0);
        co_viec = true;
    }
    control_packet packet;
    if (xQueueReceive(service->input_queue, &packet, 0) == pdPASS) {
        if (packet.is_button)
            packet.event = translate_button(&service->state, packet.button);
        control_response part = {
            .command_id = packet.command_id
        };
        bool doi_dau_ra = packet.event.type == CONTROL_EVENT_SET_MODE || packet.event.type == CONTROL_EVENT_SET_RELAY;
        uint64_t now = get_time_ms();
        if (packet.from_web && (now < packet.received_time_ms || now - packet.received_time_ms >= 1000 ||
                              (doi_dau_ra && packet.control_revision != service->control_revision))) {
            part.result_event = process(service, &tick, packet.command_id);
            part.result_event.event = packet.event.type;
            part.result_event.result_code = CONTROL_STALE;
            ESP_LOGW(TAG, "su_kien=tu_choi_lenh_web_cu lenh=%" PRIu64, packet.command_id);
        } else
            part.result_event = process(service, &packet.event, packet.command_id);
        if (packet.from_web)
            ESP_LOGI(TAG,
                "nguon=web lenh=%" PRIu64 " su_kien=%d ket_qua=%d",
                packet.command_id,
                packet.event.type,
                part.result_event.result_code);
        if (packet.is_button)
            ESP_LOGI(TAG,
                "nguon=nut nut=%d ket_qua=%d ban_nhap=%d nguong=%d",
                packet.button,
                part.result_event.result_code,
                service->state.draft,
                service->state.threshold);
        part.output_error = service->output_error;
        if (packet.command_id != 0) {
            /* Producer đã giữ một chỗ trước khi nhận lệnh. */
            check_internal(xQueueSend(service->response, &part, 0));
        }
        co_viec = true;
    }
    phat_ban_chup(service);
    return co_viec;
}

static TickType_t control_wait_ticks(const control_service_t *service)
{
    uint64_t now_ms = get_time_ms();
    uint64_t deadline = control_next_deadline(&service->state);
    uint64_t cho_ms = deadline <= now_ms ? 0 : deadline - now_ms;
    if (cho_ms > CHO_TOI_DA_MS)
        cho_ms = CHO_TOI_DA_MS;

    /* Làm tròn lên một tick: không quay vòng bận khi còn dưới một tick. */
    return (TickType_t)((cho_ms * configTICK_RATE_HZ + 999) / 1000);
}

static void display_task_loop(void *parameters)
{
    control_service_t *service = parameters;
    for (;;) {
        if (control_run_once(service)) {
            /* Nhường CPU một tick khi có tải liên tục, tránh làm đói idle. */
            vTaskDelay(1);
        } else {
            (void)ulTaskNotifyTake(pdTRUE, control_wait_ticks(service));
        }
    }
}

static void cleanup_init_error(control_service_t *service)
{
    QueueHandle_t queue[] = { service->input_queue, service->response, service->lcd, service->web, service->storage_jobs, service->storage_results };
    for (unsigned i = 0; i < sizeof(queue) / sizeof(queue[0]); ++i)
        if (queue[i] != NULL)
            vQueueDelete(queue[i]);
    if (service->response_slots != NULL)
        vSemaphoreDelete(service->response_slots);
    free(service);
}

esp_err_t control_task_start(control_service_t **out, const control_task_config *config)
{
    if (out == NULL || config == NULL || config->set_relay == NULL)
        return ESP_ERR_INVALID_ARG;

    *out = NULL;
    control_service_t *service = calloc(1, sizeof(*service));
    if (service == NULL)
        return ESP_ERR_NO_MEM;
    service->setup = *config;
    service->control_revision = 1;

    if (control_initialize(&service->state,
            &config->policy,
            config->boot_id,
            get_time_ms(),
            config->saved_data_valid,
            config->saved_threshold) != CONTROL_OK) {
        free(service);
        return ESP_ERR_INVALID_ARG;
    }

    service->input_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(control_packet));
    service->response = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(control_response));
    service->lcd = xQueueCreate(1, sizeof(control_snapshot));
    service->web = xQueueCreate(1, sizeof(control_snapshot));
    service->storage_jobs = xQueueCreate(1, sizeof(storage_job));
    service->storage_results = xQueueCreate(1, sizeof(control_event));
    service->response_slots = xSemaphoreCreateCounting(COMMAND_QUEUE_LENGTH, COMMAND_QUEUE_LENGTH);
    if (!service->input_queue || !service->response || !service->lcd || !service->web || !service->storage_jobs || !service->storage_results ||
        !service->response_slots) {
        cleanup_init_error(service);
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t error = config->set_relay(false, config->context);
    if (error != ESP_OK) {
        cleanup_init_error(service);
        return error;
    }
    ESP_LOGI(TAG,
        "su_kien=khoi_dong che_do=AUTO relay=TAT cam_bien=DANG_ON_DINH nguong=%d "
        "cau_hinh_hop_le=%d",
        service->state.threshold,
        service->state.configuration_valid);
    phat_ban_chup(service);
    if (xTaskCreate(display_task_loop, "dieu_khien", NGAN_XEP_BYTE, service, UU_TIEN, &service->task_handle) != pdPASS) {
        cleanup_init_error(service);
        return ESP_ERR_NO_MEM;
    }
    *out = service;
    return ESP_OK;
}

static esp_err_t send_command(control_service_t *service, uint64_t id, const control_event *enable, bool web, uint64_t revision)
{
    if (service == NULL || enable == NULL || id == 0 || enable->type < CONTROL_EVENT_SET_MODE || enable->type > CONTROL_EVENT_SAVE_DRAFT)
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(service->response_slots, 0) != pdPASS) {
        ESP_LOGW(TAG, "su_kien=het_cho_phan_hoi lenh=%" PRIu64, id);
        return ESP_ERR_TIMEOUT;
    }
    control_packet packet = {
        .command_id = id,
        .event = *enable,
        .from_web = web,
        .received_time_ms = get_time_ms(),
        .control_revision = revision
    };
    if (xQueueSend(service->input_queue, &packet, 0) != pdPASS) {
        check_internal(xSemaphoreGive(service->response_slots));
        ESP_LOGW(TAG, "su_kien=hang_dau_vao_day lenh=%" PRIu64, id);
        return ESP_ERR_TIMEOUT;
    }
    xTaskNotifyGive(service->task_handle);
    return ESP_OK;
}

esp_err_t control_send_command(control_service_t *service, uint64_t id, const control_event *enable)
{
    return send_command(service, id, enable, false, 0);
}

esp_err_t control_send_web_command(control_service_t *service, uint64_t id, const control_event *enable, uint64_t revision)
{
    return send_command(service, id, enable, true, revision);
}

esp_err_t control_send_sample(control_service_t *service, const control_event *enable)
{
    if (service == NULL || enable == NULL || enable->type != CONTROL_EVENT_NEW_SAMPLE)
        return ESP_ERR_INVALID_ARG;
    control_packet packet = {
        .event = *enable
    };
    if (xQueueSend(service->input_queue, &packet, 0) != pdPASS) {
        ESP_LOGW(TAG, "su_kien=mat_mau thu_tu=%" PRIu64, enable->sequence);
        return ESP_ERR_TIMEOUT;
    }
    xTaskNotifyGive(service->task_handle);
    return ESP_OK;
}

esp_err_t control_read_response(control_service_t *service, control_response *session)
{
    if (!service || !session)
        return ESP_ERR_INVALID_ARG;
    if (xQueueReceive(service->response, session, 0) != pdPASS)
        return ESP_ERR_NOT_FOUND;
    check_internal(xSemaphoreGive(service->response_slots));
    return ESP_OK;
}

esp_err_t control_read_lcd_snapshot(control_service_t *service, control_snapshot *snapshot)
{
    if (!service || !snapshot)
        return ESP_ERR_INVALID_ARG;
    return xQueuePeek(service->lcd, snapshot, 0) == pdPASS ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t control_read_web_snapshot(control_service_t *service, control_snapshot *snapshot)
{
    if (!service || !snapshot)
        return ESP_ERR_INVALID_ARG;
    return xQueuePeek(service->web, snapshot, 0) == pdPASS ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t control_take_storage_job(control_service_t *service, storage_job *v)
{
    if (!service || !v)
        return ESP_ERR_INVALID_ARG;
    return xQueueReceive(service->storage_jobs, v, 0) == pdPASS ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t control_report_storage_result(control_service_t *service, const control_event *enable)
{
    if (!service || !enable || (enable->type != CONTROL_EVENT_SAVE_OK && enable->type != CONTROL_EVENT_SAVE_FAILED))
        return ESP_ERR_INVALID_ARG;
    if (xQueueSend(service->storage_results, enable, 0) != pdPASS) {
        ESP_LOGW(TAG, "su_kien=hang_ket_qua_luu_day id=%" PRIu64, enable->save_id);
        return ESP_ERR_TIMEOUT; /* Storage giữ kết quả và thử lại, không bỏ. */
    }
    xTaskNotifyGive(service->task_handle);
    return ESP_OK;
}
