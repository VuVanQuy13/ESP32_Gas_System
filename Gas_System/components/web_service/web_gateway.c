#include "web_gateway.h"
#include "cJSON.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static bool parse_integer(const cJSON *j, int minimum, int maximum, int *out)
{
    if (!cJSON_IsNumber(j) || !isfinite(j->valuedouble) || j->valuedouble < minimum ||
        j->valuedouble > maximum || floor(j->valuedouble) != j->valuedouble)
        return false;
    *out = (int)j->valuedouble;
    return true;
}

static bool parse_hex64(const cJSON *j, uint64_t *out)
{
    if (!cJSON_IsString(j) || strlen(j->valuestring) != 16)
        return false;
    uint64_t v = 0;
    for (unsigned i = 0; i < 16; ++i) {
        char config = j->valuestring[i];
        unsigned length;
        if (config >= '0' && config <= '9')
            length = (unsigned)(config - '0');
        else if (config >= 'a' && config <= 'f')
            length = (unsigned)(config - 'a' + 10);
        else
            return false;
        v = (v << 4) | length;
    }
    *out = v;
    return true;
}

static const cJSON *get_field(const cJSON *j, const char *ten)
{
    return cJSON_GetObjectItemCaseSensitive(j, ten);
}

static bool parse_command(const char *text, uint32_t *id, control_event *enable, uint64_t *revision)
{
    cJSON *j = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, NULL, true);
    bool valid = false;
    int number = 0;
    if (!cJSON_IsObject(j))
        goto xong;

    /* Từ chối trường lạ/trùng; giới hạn độ sâu thực tế bằng schema phẳng. */
    const char *ten[] = { "v", "id", "type", "boot", "control", "edit", "revision", "value" };
    unsigned mask = 0;
    for (const cJSON *session = j->child; session; session = session->next) {
        unsigned i = 0;
        while (i < 8 && strcmp(session->string, ten[i]))
            ++i;
        if (i == 8 || (mask & (1U << i)))
            goto xong;
        mask |= 1U << i;
    }

    if (!parse_integer(get_field(j, "v"), 1, 1, &number) || !parse_integer(get_field(j, "id"), 1, 2147483647, &number))
        goto xong;
    *id = (uint32_t)number;
    if (!parse_hex64(get_field(j, "boot"), &enable->boot_id) || !enable->boot_id)
        goto xong;
    const cJSON *worker = get_field(j, "type");
    if (!cJSON_IsString(worker))
        goto xong;
    unsigned can = 15;
    if (!strcmp(worker->valuestring, "SET_MODE") || !strcmp(worker->valuestring, "SET_RELAY")) {
        enable->type = !strcmp(worker->valuestring, "SET_MODE") ? CONTROL_EVENT_SET_MODE : CONTROL_EVENT_SET_RELAY;
        if (!parse_integer(get_field(j, "value"), 0, 1, &enable->value) ||
            !parse_hex64(get_field(j, "control"), revision))
            goto xong;
        can |= (1U << 4) | (1U << 7);
    } else if (!strcmp(worker->valuestring, "BEGIN_EDIT"))
        enable->type = CONTROL_EVENT_BEGIN_EDIT;
    else if (!strcmp(worker->valuestring, "SET_DRAFT") || !strcmp(worker->valuestring, "SAVE_DRAFT")) {
        enable->type = !strcmp(worker->valuestring, "SET_DRAFT") ? CONTROL_EVENT_SET_DRAFT : CONTROL_EVENT_SAVE_DRAFT;
        if (!parse_hex64(get_field(j, "edit"), &enable->edit_session_id) ||
            !parse_hex64(get_field(j, "revision"), &enable->expected_revision))
            goto xong;
        can |= (1U << 5) | (1U << 6);
        if (enable->type == CONTROL_EVENT_SET_DRAFT) {
            if (!parse_integer(get_field(j, "value"), 1, 4095, &enable->value))
                goto xong;
            can |= 1U << 7;
        }
    } else
        goto xong;
    valid = mask == can;
xong:
    cJSON_Delete(j);
    return valid;
}

static void format_hex(uint64_t number, char out[17])
{
    const char text[] = "0123456789abcdef";
    for (unsigned i = 0; i < 16; ++i) {
        out[15 - i] = text[number & 15];
        number >>= 4;
    }
    out[16] = '\0';
}

static esp_err_t result_event(char *out, size_t length, uint32_t id, const char *tt, int code, uint64_t lan)
{
    char hex[17];
    format_hex(lan, hex);
    int written = snprintf(out,
        length,
        "{\"type\":\"result\",\"id\":%" PRIu32 ",\"status\":\"%s\",\"code\":%d,\"save_id\":\"%s\"}",
        id,
        tt,
        code,
        hex);
    return written >= 0 && (size_t)written < length ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t web_receive_command(web_cong *gateway, unsigned result, const char *text, char *out, size_t length)
{
    if (!gateway || !gateway->service || result >= WEB_SO_KHACH || !text || !out || length < 192)
        return ESP_ERR_INVALID_ARG;
    control_event enable = { 0 };
    uint64_t revision = 0;
    uint32_t id = 0;
    unsigned open_count = 0, close_count = 0;
    for (const char *session = text; *session; ++session) {
        if (*session == '{')
            ++open_count;
        if (*session == '}')
            ++close_count;
    }

    /* Không chấp nhận escape trong giao thức phẳng để tránh alias khóa/chuỗi NUL. */
    if (strlen(text) > WEB_LENH_TOI_DA || open_count != 1 || close_count != 1 || strchr(text, '[') ||
        strchr(text, ']') || strchr(text, '\\') || !parse_command(text, &id, &enable, &revision))
        return result_event(out, length, id, "INVALID", CONTROL_INVALID, 0);
    web_session *session = &gateway->clients[result];
    if (id == session->id) {
        if (strcmp(text, session->command))
            return result_event(out, length, id, "ID_CONFLICT", CONTROL_STALE, 0);
        memcpy(out, session->reply, strlen(session->reply) + 1);
        return ESP_OK;
    }

    if (id < session->id)
        return result_event(out, length, id, "OLD_ID", CONTROL_STALE, 0);
    if (session->pending_id)
        return result_event(out, length, id, "BUSY", CONTROL_BUSY, 0);
    if (gateway->internal_id == UINT64_MAX)
        return ESP_ERR_INVALID_STATE;
    session->id = id;
    memcpy(session->command, text, strlen(text) + 1);
    esp_err_t error = control_send_web_command(gateway->service, ++gateway->internal_id, &enable, revision);
    session->pending_id = error == ESP_OK ? gateway->internal_id : 0;
    esp_err_t tao =
        result_event(session->reply, sizeof(session->reply), id, error == ESP_OK ? "QUEUED" : "BUSY", error, 0);
    if (tao != ESP_OK)
        return tao;
    memcpy(out, session->reply, strlen(session->reply) + 1);
    return ESP_OK;
}

esp_err_t web_drain_responses(web_cong *gateway)
{
    if (!gateway || !gateway->service)
        return ESP_ERR_INVALID_ARG;
    for (unsigned i = 0; i < 16; ++i) {
        control_response session;
        esp_err_t error = control_read_response(gateway->service, &session);
        if (error == ESP_ERR_NOT_FOUND)
            return ESP_OK;
        if (error != ESP_OK)
            return error;
        for (unsigned result = 0; result < WEB_SO_KHACH; ++result)
            if (gateway->clients[result].pending_id == session.command_id) {
                web_session *state = &gateway->clients[result];
                state->pending_id = 0;
                const char *tt = session.result_event.result_code != CONTROL_OK ? "REJECTED"
                                 : session.output_error != ESP_OK              ? "OUTPUT_ERROR"
                                 : session.result_event.needs_save                   ? "PENDING"
                                                                       : "APPLIED";
                error = result_event(state->reply,
                    sizeof(state->reply),
                    state->id,
                    tt,
                    session.result_event.result_code,
                    session.result_event.save_id);
                if (error != ESP_OK)
                    return error;
            }
    }

    return ESP_OK;
}

esp_err_t web_tao_ban_tin(const control_snapshot *snapshot, uint64_t now, char *out, size_t length)
{
    if (!snapshot || !out || !length || !snapshot->state.initialized)
        return ESP_ERR_INVALID_ARG;
    const control_state *state = &snapshot->state;
    bool cu = now < state->current_time_ms || now - state->current_time_ms >= 1000;
    uint64_t tuoi = state->has_sample && now >= state->sample_time_ms ? now - state->sample_time_ms : UINT64_MAX;
    char value[16] = "null";
    if (!cu && state->sensor_status == SENSOR_READY && tuoi < state->config.data_timeout_ms)
        snprintf(value, sizeof(value), "%d", state->sample);
    char hex[6][17];
    format_hex(state->boot_id, hex[0]);
    format_hex(snapshot->revision, hex[1]);
    format_hex(snapshot->control_revision, hex[2]);
    format_hex(state->edit_session_id, hex[3]);
    format_hex(state->draft_revision, hex[4]);
    format_hex(state->save_id, hex[5]);
    int written = snprintf(out,
        length,
        "{\"type\":\"state\",\"v\":2,\"boot\":\"%s\",\"seq\":\"%s\",\"control\":\"%"
        "s\",\"edit\":\"%s\",\"revision\":\"%s\",\"save_id\":\"%s\",\"mode\":%d,"
        "\"sensor\":%d,\"relay\":%d,\"relay_error\":%d,\"save_error\":%d,"
        "\"threshold\":%d,\"draft\":%d,\"editing\":%d,\"unknown\":%d,\"config\":%d,"
        "\"fire_valid\":%d,\"fire\":%d,\"fire_confirming\":%d,"
        "\"value\":%s,\"stale\":%d,"
        "\"minimum\":%d,\"maximum\":%d,\"step\":10}",
        hex[0],
        hex[1],
        hex[2],
        hex[3],
        hex[4],
        hex[5],
        state->mode,
        state->sensor_status,
        state->relay_on,
        snapshot->output_error,
        snapshot->storage_error,
        state->threshold,
        state->draft,
        state->edit_status,
        state->save_result_unknown,
        state->configuration_valid,
        state->fire_read_valid,
        state->fire_detected,
        state->fire_clear_confirmation_active,
        value,
        cu,
        state->config.minimum_threshold,
        state->config.maximum_threshold);
    return written >= 0 && (size_t)written < length ? ESP_OK : ESP_ERR_NO_MEM;
}
