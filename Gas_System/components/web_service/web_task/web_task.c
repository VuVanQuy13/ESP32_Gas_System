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
    web_cong cong;
    int fd[WEB_SO_KHACH];
    atomic_bool da_xep;
    TaskHandle_t tac_vu;
} dich_vu;

extern const unsigned char html_dau[] asm("_binary_index_html_start");
extern const unsigned char html_cuoi[] asm("_binary_index_html_end");
extern const unsigned char js_dau[] asm("_binary_app_js_start");
extern const unsigned char js_cuoi[] asm("_binary_app_js_end");
extern const unsigned char css_dau[] asm("_binary_style_css_start");
extern const unsigned char css_cuoi[] asm("_binary_style_css_end");

static esp_err_t tep(httpd_req_t *r)
{
    const unsigned char *d = html_dau, *c = html_cuoi;
    const char *loai = "text/html; charset=utf-8";
    if (!strcmp(r->uri, "/app.js")) {
        d = js_dau;
        c = js_cuoi;
        loai = "application/javascript; charset=utf-8";
    } else if (!strcmp(r->uri, "/style.css")) {
        d = css_dau;
        c = css_cuoi;
        loai = "text/css; charset=utf-8";
    }
    esp_err_t loi = httpd_resp_set_type(r, loai);
    if (loi == ESP_OK)
        loi = httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    if (loi == ESP_OK)
        loi = httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    if (loi == ESP_OK)
        loi = httpd_resp_set_hdr(r,
            "Content-Security-Policy",
            "default-src 'self'; connect-src 'self'; frame-ancestors 'none'");
    return loi == ESP_OK ? httpd_resp_send(r, (const char *)d, c - d) : loi;
}

static esp_err_t kiem_tra_origin(httpd_req_t *r)
{
    char host[96], origin[112], mong_doi[112];
    if (httpd_req_get_hdr_value_str(r, "Host", host, sizeof(host)) != ESP_OK ||
        httpd_req_get_hdr_value_str(r, "Origin", origin, sizeof(origin)) != ESP_OK)
        return ESP_FAIL;
    int n = snprintf(mong_doi, sizeof(mong_doi), "http://%s", host);
    return n > 0 && (size_t)n < sizeof(mong_doi) && !strcmp(origin, mong_doi) ? ESP_OK : ESP_FAIL;
}

static esp_err_t gui(int fd, const char *chu)
{
    httpd_ws_frame_t f = {
        .final = true,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)chu,
        .len = strlen(chu)
    };
    return httpd_ws_send_frame_async(dich_vu.server, fd, &f);
}

static void bo_khach(unsigned i)
{
    dich_vu.fd[i] = -1;
    dich_vu.cong.khach[i] = (web_phien){ 0 };
}

static esp_err_t ws(httpd_req_t *r)
{
    int fd = httpd_req_to_sockfd(r), vi_tri = -1;
    for (unsigned i = 0; i < WEB_SO_KHACH; ++i)
        if (dich_vu.fd[i] == fd)
            vi_tri = (int)i;
    if (r->method == HTTP_GET) {
        if (vi_tri < 0)
            for (unsigned i = 0; i < WEB_SO_KHACH; ++i) {
                if (dich_vu.fd[i] < 0 ||
                    httpd_ws_get_fd_info(r->handle, dich_vu.fd[i]) != HTTPD_WS_CLIENT_WEBSOCKET) {
                    vi_tri = (int)i;
                    break;
                }
            }

        if (vi_tri < 0)
            return ESP_FAIL;
        bo_khach((unsigned)vi_tri);
        dich_vu.fd[vi_tri] = fd;
        ESP_LOGI("web", "su_kien=ket_noi khach=%d", vi_tri);
        return ESP_OK; /* Broadcast tiếp theo gửi snapshot, không dùng fd như định danh lệnh. */
    }

    if (vi_tri < 0)
        return ESP_FAIL;
    httpd_ws_frame_t f = { 0 };
    esp_err_t loi = httpd_ws_recv_frame(r, &f, 0);
    if (loi != ESP_OK)
        return loi;
    if (f.type != HTTPD_WS_TYPE_TEXT || !f.final || !f.len || f.len > WEB_LENH_TOI_DA)
        return ESP_FAIL;
    char chu[WEB_LENH_TOI_DA + 1], tra[192];
    f.payload = (uint8_t *)chu;
    loi = httpd_ws_recv_frame(r, &f, WEB_LENH_TOI_DA);
    if (loi != ESP_OK)
        return loi;
    chu[f.len] = '\0';
    if (strlen(chu) != f.len)
        return ESP_FAIL;
    loi = web_don_phan_hoi(&dich_vu.cong);
    if (loi == ESP_OK)
        loi = web_nhan_lenh(&dich_vu.cong, (unsigned)vi_tri, chu, tra, sizeof(tra));
    return loi == ESP_OK ? gui(fd, tra) : loi;
}

static void phat(void *ctx)
{
    (void)ctx;
    esp_err_t loi = web_don_phan_hoi(&dich_vu.cong);
    dk_ban_chup b;
    char chu[WEB_BAN_TIN_TOI_DA];
    if (loi == ESP_OK)
        loi = dk_doc_web(dich_vu.cong.dich_vu, &b);
    if (loi == ESP_OK)
        loi = web_tao_ban_tin(&b, (uint64_t)(esp_timer_get_time() / 1000), chu, sizeof(chu));
    if (loi == ESP_OK)
        for (unsigned i = 0; i < WEB_SO_KHACH; ++i) {
            int fd = dich_vu.fd[i];
            if (fd < 0)
                continue;
            if (httpd_ws_get_fd_info(dich_vu.server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) {
                bo_khach(i);
                continue;
            }
            web_phien *p = &dich_vu.cong.khach[i];
            esp_err_t gui_loi = gui(fd, chu);
            if (gui_loi == ESP_OK && p->id)
                gui_loi = gui(fd, p->tra_loi);
            if (gui_loi != ESP_OK) {
                ESP_LOGW("web", "su_kien=ngat_khach loi=%d", gui_loi);
                esp_err_t dong = httpd_sess_trigger_close(dich_vu.server, fd);
                if (dong != ESP_OK)
                    ESP_LOGW("web", "su_kien=loi_dong_socket loi=%d", dong);
                bo_khach(i);
            }
        }
    else
        ESP_LOGE("web", "su_kien=loi_snapshot loi=%d", loi);
    atomic_store(&dich_vu.da_xep, false);
}

static void nhip_web(void *ctx)
{
    (void)ctx;
    for (;;) {
        if (!atomic_exchange(&dich_vu.da_xep, true)) {
            esp_err_t loi = httpd_queue_work(dich_vu.server, phat, NULL);
            if (loi != ESP_OK) {
                atomic_store(&dich_vu.da_xep, false);
                ESP_LOGW("web", "su_kien=khong_xep_duoc_cong_viec loi=%d", loi);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200) > 0 ? pdMS_TO_TICKS(200) : 1);
    }
}

esp_err_t web_bat_dau(dk_dich_vu *d)
{
    if (!d)
        return ESP_ERR_INVALID_ARG;
    if (dich_vu.server)
        return ESP_ERR_INVALID_STATE;
    dich_vu.cong.dich_vu = d;
    for (unsigned i = 0; i < WEB_SO_KHACH; ++i)
        bo_khach(i);
    atomic_init(&dich_vu.da_xep, false);
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.stack_size = 6144;
    c.max_open_sockets = 5;
    c.lru_purge_enable = true;
    c.recv_wait_timeout = 1;
    c.send_wait_timeout = 1;
    esp_err_t loi = httpd_start(&dich_vu.server, &c);
    if (loi != ESP_OK)
        return loi;
    const httpd_uri_t uris[] = { {
        .uri = "/",
        .method = HTTP_GET,
        .handler = tep
    },
        {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = tep
        },
        {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = tep
        },
        {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = ws,
            .is_websocket = true,
            .ws_pre_handshake_cb = kiem_tra_origin
        } };
    for (unsigned i = 0; i < sizeof(uris) / sizeof(uris[0]); ++i) {
        loi = httpd_register_uri_handler(dich_vu.server, &uris[i]);
        if (loi != ESP_OK)
            break;
    }

    if (loi == ESP_OK &&
        xTaskCreate(nhip_web, "web_nhip", 3072, NULL, 2, &dich_vu.tac_vu) != pdPASS)
        loi = ESP_ERR_NO_MEM;
    if (loi != ESP_OK) {
        esp_err_t dong = httpd_stop(dich_vu.server);
        if (dong == ESP_OK)
            dich_vu.server = NULL;
        else
            ESP_LOGE("web", "su_kien=loi_dung_http loi=%d", dong);
    }

    if (loi == ESP_OK)
        ESP_LOGI("web", "su_kien=http_san_sang cong=%u websocket=/ws", (unsigned)c.server_port);
    return loi;
}
#else
esp_err_t web_bat_dau(dk_dich_vu *d)
{
    (void)d;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
