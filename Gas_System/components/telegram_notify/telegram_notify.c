#include "telegram_notify.h"

#include "sdkconfig.h"

#if CONFIG_GAS_TELEGRAM_ENABLED

#include "telegram_notify_format.h"
#include "telegram_notify_pending.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TELEGRAM_TASK_STACK_SIZE 8192
#define TELEGRAM_TASK_PRIORITY 4
#define TELEGRAM_HTTP_TIMEOUT_MS 10000
#define TELEGRAM_RESPONSE_MAX 1024
#define TELEGRAM_TOKEN_MAX 128
#define TELEGRAM_CHAT_ID_MAX 64

static const char *TAG = "telegram_notify";

typedef struct {
    char data[TELEGRAM_RESPONSE_MAX + 1];
    size_t length;
    bool overflow;
} telegram_response_buffer;

static QueueHandle_t event_queue;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static bool service_started;
static bool event_in_progress;
static telegram_notify_pending_state pending_state;

static bool is_valid_token(const char *token)
{
    if (token == NULL || token[0] == '\0' || strlen(token) > TELEGRAM_TOKEN_MAX) {
        return false;
    }

    for (const char *character = token; *character != '\0'; character++) {
        const bool allowed =
            (*character >= 'a' && *character <= 'z') ||
            (*character >= 'A' && *character <= 'Z') ||
            (*character >= '0' && *character <= '9') ||
            *character == ':' || *character == '_' || *character == '-';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

static bool is_valid_chat_id(const char *chat_id)
{
    if (chat_id == NULL || chat_id[0] == '\0' || strlen(chat_id) > TELEGRAM_CHAT_ID_MAX) {
        return false;
    }

    for (const char *character = chat_id; *character != '\0'; character++) {
        const bool allowed =
            (*character >= 'a' && *character <= 'z') ||
            (*character >= 'A' && *character <= 'Z') ||
            (*character >= '0' && *character <= '9') ||
            *character == '_' || *character == '-';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

static esp_err_t receive_http_event(esp_http_client_event_t *event)
{
    if (event == NULL || event->user_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (event->event_id != HTTP_EVENT_ON_DATA || event->data == NULL || event->data_len <= 0) {
        return ESP_OK;
    }

    telegram_response_buffer *response = event->user_data;
    const size_t received = (size_t)event->data_len;

    if (received > TELEGRAM_RESPONSE_MAX - response->length) {
        response->overflow = true;
        return ESP_OK;
    }

    memcpy(response->data + response->length, event->data, received);
    response->length += received;
    response->data[response->length] = '\0';
    return ESP_OK;
}

static bool response_reports_success(const telegram_response_buffer *response)
{
    if (response == NULL || response->overflow || response->length == 0) {
        return false;
    }

    cJSON *root = cJSON_Parse(response->data);
    if (root == NULL) {
        return false;
    }

    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
    const bool successful = cJSON_IsTrue(ok);
    cJSON_Delete(root);
    return successful;
}

static esp_err_t send_event_once(const telegram_notify_event *event)
{
    char message[TELEGRAM_NOTIFY_MESSAGE_MAX];
    const int message_length = telegram_notify_format_message(
        event,
        message,
        sizeof(message)
    );

    if (message_length < 0) {
        ESP_LOGE(TAG, "event=send result=drop reason=invalid_message");
        return ESP_ERR_INVALID_ARG;
    }

    const char *token = CONFIG_GAS_TELEGRAM_BOT_TOKEN;
    const char *chat_id = CONFIG_GAS_TELEGRAM_CHAT_ID;

    if (!is_valid_token(token) || !is_valid_chat_id(chat_id)) {
        ESP_LOGE(TAG, "event=send result=drop reason=invalid_credentials");
        return ESP_ERR_INVALID_ARG;
    }

    const size_t path_size = strlen(token) + sizeof("/bot/sendMessage");
    char *path = malloc(path_size);
    if (path == NULL) {
        ESP_LOGE(TAG, "event=send result=drop reason=no_memory");
        return ESP_ERR_NO_MEM;
    }

    const int path_length = snprintf(path, path_size, "/bot%s/sendMessage", token);
    if (path_length < 0 || (size_t)path_length >= path_size) {
        free(path);
        ESP_LOGE(TAG, "event=send result=drop reason=path_format");
        return ESP_FAIL;
    }

    cJSON *request = cJSON_CreateObject();
    if (request == NULL ||
        !cJSON_AddStringToObject(request, "chat_id", chat_id) ||
        !cJSON_AddStringToObject(request, "text", message)) {
        cJSON_Delete(request);
        free(path);
        ESP_LOGE(TAG, "event=send result=drop reason=no_memory");
        return ESP_ERR_NO_MEM;
    }

    char *request_body = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (request_body == NULL) {
        free(path);
        ESP_LOGE(TAG, "event=send result=drop reason=no_memory");
        return ESP_ERR_NO_MEM;
    }

    telegram_response_buffer response = {0};
    const esp_http_client_config_t config = {
        .host = "api.telegram.org",
        .path = path,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = TELEGRAM_HTTP_TIMEOUT_MS,
        .disable_auto_redirect = true,
        .max_authorization_retries = -1,
        .event_handler = receive_http_event,
        .user_data = &response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    free(path);
    if (client == NULL) {
        free(request_body);
        ESP_LOGE(TAG, "event=send result=drop reason=http_client_init");
        return ESP_FAIL;
    }

    esp_err_t error = esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (error == ESP_OK) {
        error = esp_http_client_set_header(client, "Content-Type", "application/json");
    }
    if (error == ESP_OK) {
        error = esp_http_client_set_post_field(client, request_body, strlen(request_body));
    }

    int status_code = 0;
    if (error == ESP_OK) {
        error = esp_http_client_perform(client);
        status_code = esp_http_client_get_status_code(client);
    }

    const bool api_success =
        error == ESP_OK && status_code >= 200 && status_code < 300 &&
        response_reports_success(&response);

    esp_http_client_cleanup(client);
    free(request_body);

    if (api_success) {
        ESP_LOGI(
            TAG,
            "event=send result=success transitions=0x%lx gas_data=%d adc=%d threshold=%d http=%d",
            (unsigned long)event->transitions,
            event->has_gas_reading,
            event->adc_value,
            event->threshold,
            status_code
        );
        return ESP_OK;
    }

    ESP_LOGE(
        TAG,
        "event=send result=drop transitions=0x%lx gas_data=%d adc=%d threshold=%d http=%d error=%s api_ok=%d",
        (unsigned long)event->transitions,
        event->has_gas_reading,
        event->adc_value,
        event->threshold,
        status_code,
        esp_err_to_name(error),
        response_reports_success(&response)
    );
    return error == ESP_OK ? ESP_FAIL : error;
}

static void telegram_worker(void *argument)
{
    (void)argument;

    telegram_notify_event event;
    while (true) {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        bool has_pending;
        do {
            send_event_once(&event);

            portENTER_CRITICAL(&state_lock);
            has_pending = telegram_notify_pending_take(&pending_state, &event);
            if (!has_pending) {
                event_in_progress = false;
            }
            portEXIT_CRITICAL(&state_lock);
        } while (has_pending);
    }
}

esp_err_t telegram_notify_start(void)
{
    portENTER_CRITICAL(&state_lock);
    const bool already_started = service_started;
    portEXIT_CRITICAL(&state_lock);
    if (already_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!is_valid_token(CONFIG_GAS_TELEGRAM_BOT_TOKEN) ||
        !is_valid_chat_id(CONFIG_GAS_TELEGRAM_CHAT_ID)) {
        ESP_LOGE(TAG, "event=start result=failed reason=invalid_credentials");
        return ESP_ERR_INVALID_ARG;
    }

    QueueHandle_t queue = xQueueCreate(1, sizeof(telegram_notify_event));
    if (queue == NULL) {
        ESP_LOGE(TAG, "event=start result=failed reason=queue_create");
        return ESP_ERR_NO_MEM;
    }

    portENTER_CRITICAL(&state_lock);
    event_queue = queue;
    portEXIT_CRITICAL(&state_lock);

    if (xTaskCreate(
            telegram_worker,
            "telegram_notify",
            TELEGRAM_TASK_STACK_SIZE,
            NULL,
            TELEGRAM_TASK_PRIORITY,
            NULL
        ) != pdPASS) {
        portENTER_CRITICAL(&state_lock);
        event_queue = NULL;
        portEXIT_CRITICAL(&state_lock);
        vQueueDelete(queue);
        ESP_LOGE(TAG, "event=start result=failed reason=task_create");
        return ESP_ERR_NO_MEM;
    }

    portENTER_CRITICAL(&state_lock);
    event_in_progress = false;
    memset(&pending_state, 0, sizeof(pending_state));
    service_started = true;
    portEXIT_CRITICAL(&state_lock);

    ESP_LOGI(TAG, "event=start result=success");
    return ESP_OK;
}

esp_err_t telegram_notify_submit(const telegram_notify_event *event)
{
    const uint32_t gas_transitions =
        TELEGRAM_NOTIFY_GAS_ALARM | TELEGRAM_NOTIFY_GAS_RECOVERY;
    const bool has_gas_transition =
        event != NULL && (event->transitions & gas_transitions) != 0;
    if (event == NULL || event->transitions == 0 ||
        (event->transitions & ~TELEGRAM_NOTIFY_ALL_TRANSITIONS) != 0 ||
        ((event->transitions & gas_transitions) == gas_transitions) ||
        ((event->transitions & (TELEGRAM_NOTIFY_FIRE_DETECTED | TELEGRAM_NOTIFY_FIRE_CLEARED)) ==
            (TELEGRAM_NOTIFY_FIRE_DETECTED | TELEGRAM_NOTIFY_FIRE_CLEARED)) ||
        event->has_gas_reading != has_gas_transition ||
        (has_gas_transition &&
            (event->adc_value < 0 || event->adc_value > 4095 ||
                event->threshold < 0 || event->threshold > 4095))) {
        ESP_LOGE(TAG, "event=submit result=drop reason=invalid_event");
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&state_lock);
    if (!service_started) {
        portEXIT_CRITICAL(&state_lock);
        ESP_LOGE(TAG, "event=submit result=drop reason=service_not_started");
        return ESP_ERR_INVALID_STATE;
    }

    if (event_in_progress) {
        const bool held = telegram_notify_pending_handle_busy(&pending_state, event);
        portEXIT_CRITICAL(&state_lock);
        if (held) {
            ESP_LOGI(TAG,
                "event=submit result=held_for_send transitions=0x%lx",
                (unsigned long)event->transitions);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "event=submit result=drop reason=worker_busy");
        return ESP_ERR_TIMEOUT;
    }

    event_in_progress = true;
    QueueHandle_t queue = event_queue;
    portEXIT_CRITICAL(&state_lock);

    const BaseType_t queued = xQueueSend(queue, event, 0);
    if (queued != pdTRUE) {
        portENTER_CRITICAL(&state_lock);
        event_in_progress = false;
        portEXIT_CRITICAL(&state_lock);
    }

    if (queued != pdTRUE) {
        ESP_LOGW(TAG, "event=submit result=drop reason=queue_full");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(
        TAG,
        "event=submit result=accepted transitions=0x%lx gas_data=%d adc=%d threshold=%d",
        (unsigned long)event->transitions,
        event->has_gas_reading,
        event->adc_value,
        event->threshold
    );
    return ESP_OK;
}

#else

esp_err_t telegram_notify_start(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t telegram_notify_submit(const telegram_notify_event *event)
{
    (void)event;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
