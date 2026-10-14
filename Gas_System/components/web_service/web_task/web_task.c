#include "web_service.h"
#include "web_gateway.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#if CONFIG_GAS_WEB
static struct {
    httpd_handle_t server;
    web_cong gateway;
    int fd[WEB_SO_KHACH];
    atomic_bool queued;
    TaskHandle_t task_handle;
} web_runtime;

extern const unsigned char html_dau[] asm("_binary_index_html_start");
extern const unsigned char html_cuoi[] asm("_binary_index_html_end");
extern const unsigned char js_dau[] asm("_binary_app_js_start");
extern const unsigned char js_cuoi[] asm("_binary_app_js_end");
extern const unsigned char css_dau[] asm("_binary_style_css_start");
extern const unsigned char css_cuoi[] asm("_binary_style_css_end");

static esp_err_t serve_file(httpd_req_t *r)
{
    const unsigned char *service = html_dau, *config = html_cuoi;
    const char *type = "text/html; charset=utf-8";
    if (!strcmp(r->uri, "/app.js")) {
        service = js_dau;
        config = js_cuoi;
        type = "application/javascript; charset=utf-8";
    } else if (!strcmp(r->uri, "/style.css")) {
        service = css_dau;
        config = css_cuoi;
        type = "text/css; charset=utf-8";
    }
    esp_err_t error = httpd_resp_set_type(r, type);
    if (error == ESP_OK)
        error = httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    if (error == ESP_OK)
        error = httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    if (error == ESP_OK)
        error = httpd_resp_set_hdr(r,
            "Content-Security-Policy",
            "default-src 'self'; connect-src 'self'; frame-ancestors 'none'");
    return error == ESP_OK ? httpd_resp_send(r, (const char *)service, config - service) : error;
}

static esp_err_t validate_origin(httpd_req_t *r)
{
    char host[96], origin[112], mong_doi[112];
    if (httpd_req_get_hdr_value_str(r, "Host", host, sizeof(host)) != ESP_OK ||
        httpd_req_get_hdr_value_str(r, "Origin", origin, sizeof(origin)) != ESP_OK)
        return ESP_FAIL;
    int length = snprintf(mong_doi, sizeof(mong_doi), "http://%s", host);
    return length > 0 && (size_t)length < sizeof(mong_doi) && !strcmp(origin, mong_doi) ? ESP_OK : ESP_FAIL;
}

static esp_err_t send_frame(int fd, const char *text)
{
    httpd_ws_frame_t f = {
        .final = true,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)text,
        .len = strlen(text)
    };
    return httpd_ws_send_frame_async(web_runtime.server, fd, &f);
}

static void remove_client(unsigned i)
{
    web_runtime.fd[i] = -1;
    web_runtime.gateway.clients[i] = (web_session){ 0 };
}

static esp_err_t websocket_handler(httpd_req_t *r)
{
    int fd = httpd_req_to_sockfd(r), index = -1;
    for (unsigned i = 0; i < WEB_SO_KHACH; ++i)
        if (web_runtime.fd[i] == fd)
            index = (int)i;
    if (r->method == HTTP_GET) {
        if (index < 0)
            for (unsigned i = 0; i < WEB_SO_KHACH; ++i) {
                if (web_runtime.fd[i] < 0 ||
                    httpd_ws_get_fd_info(r->handle, web_runtime.fd[i]) != HTTPD_WS_CLIENT_WEBSOCKET) {
                    index = (int)i;
                    break;
                }
            }

        if (index < 0)
            return ESP_FAIL;
        remove_client((unsigned)index);
        web_runtime.fd[index] = fd;
        ESP_LOGI("web", "su_kien=ket_noi khach=%d", index);
        return ESP_OK; /* Broadcast tiếp theo gửi snapshot, không dùng fd như định danh lệnh. */
    }

    if (index < 0)
        return ESP_FAIL;
    httpd_ws_frame_t f = { 0 };
    esp_err_t error = httpd_ws_recv_frame(r, &f, 0);
    if (error != ESP_OK)
        return error;
    if (f.type != HTTPD_WS_TYPE_TEXT || !f.final || !f.len || f.len > WEB_LENH_TOI_DA)
        return ESP_FAIL;
    char text[WEB_LENH_TOI_DA + 1], tra[192];
    f.payload = (uint8_t *)text;
    error = httpd_ws_recv_frame(r, &f, WEB_LENH_TOI_DA);
    if (error != ESP_OK)
        return error;
    text[f.len] = '\0';
    if (strlen(text) != f.len)
        return ESP_FAIL;
    error = web_drain_responses(&web_runtime.gateway);
    if (error == ESP_OK)
        error = web_receive_command(&web_runtime.gateway, (unsigned)index, text, tra, sizeof(tra));
    return error == ESP_OK ? send_frame(fd, tra) : error;
}

static void broadcast_state(void *ctx)
{
    (void)ctx;
    esp_err_t error = web_drain_responses(&web_runtime.gateway);
    control_snapshot snapshot;
    char text[WEB_BAN_TIN_TOI_DA];
    if (error == ESP_OK)
        error = control_read_web_snapshot(web_runtime.gateway.service, &snapshot);
    if (error == ESP_OK)
        error = web_tao_ban_tin(&snapshot, (uint64_t)(esp_timer_get_time() / 1000), text, sizeof(text));
    if (error == ESP_OK)
        for (unsigned i = 0; i < WEB_SO_KHACH; ++i) {
            int fd = web_runtime.fd[i];
            if (fd < 0)
                continue;
            if (httpd_ws_get_fd_info(web_runtime.server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) {
                remove_client(i);
                continue;
            }
            web_session *session = &web_runtime.gateway.clients[i];
            esp_err_t send_error = send_frame(fd, text);
            if (send_error == ESP_OK && session->id)
                send_error = send_frame(fd, session->reply);
            if (send_error != ESP_OK) {
                ESP_LOGW("web", "su_kien=ngat_khach loi=%d", send_error);
                esp_err_t close_count = httpd_sess_trigger_close(web_runtime.server, fd);
                if (close_count != ESP_OK)
                    ESP_LOGW("web", "su_kien=loi_dong_socket loi=%d", close_count);
                remove_client(i);
            }
        }
    else
        ESP_LOGE("web", "su_kien=loi_snapshot loi=%d", error);
    atomic_store(&web_runtime.queued, false);
}

static void web_tick_task(void *ctx)
{
    (void)ctx;
    for (;;) {
        if (!atomic_exchange(&web_runtime.queued, true)) {
            esp_err_t error = httpd_queue_work(web_runtime.server, broadcast_state, NULL);
            if (error != ESP_OK) {
                atomic_store(&web_runtime.queued, false);
                ESP_LOGW("web", "su_kien=khong_xep_duoc_cong_viec loi=%d", error);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200) > 0 ? pdMS_TO_TICKS(200) : 1);
    }
}

esp_err_t web_start(control_service_t *service)
{
    if (!service)
        return ESP_ERR_INVALID_ARG;
    if (web_runtime.server)
        return ESP_ERR_INVALID_STATE;
    web_runtime.gateway.service = service;
    for (unsigned i = 0; i < WEB_SO_KHACH; ++i)
        remove_client(i);
    atomic_init(&web_runtime.queued, false);
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 6144;
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 1;
    config.send_wait_timeout = 1;
    esp_err_t error = httpd_start(&web_runtime.server, &config);
    if (error != ESP_OK)
        return error;
    const httpd_uri_t uris[] = { {
        .uri = "/",
        .method = HTTP_GET,
        .handler = serve_file
    },
        {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = serve_file
        },
        {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = serve_file
        },
        {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = websocket_handler,
            .is_websocket = true,
            .ws_pre_handshake_cb = validate_origin
        } };
    for (unsigned i = 0; i < sizeof(uris) / sizeof(uris[0]); ++i) {
        error = httpd_register_uri_handler(web_runtime.server, &uris[i]);
        if (error != ESP_OK)
            break;
    }

    if (error == ESP_OK &&
        xTaskCreate(web_tick_task, "web_nhip", 3072, NULL, 2, &web_runtime.task_handle) != pdPASS)
        error = ESP_ERR_NO_MEM;
    if (error != ESP_OK) {
        esp_err_t close_count = httpd_stop(web_runtime.server);
        if (close_count == ESP_OK)
            web_runtime.server = NULL;
        else
            ESP_LOGE("web", "su_kien=loi_dung_http loi=%d", close_count);
    }

    if (error == ESP_OK)
        ESP_LOGI("web", "su_kien=http_san_sang cong=%u websocket=/ws", (unsigned)config.server_port);
    return error;
}
#else
esp_err_t web_start(control_service_t *service)
{
    (void)service;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
