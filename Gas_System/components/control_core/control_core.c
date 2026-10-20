#include "control_core.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define GAS_RECOVERY_REQUIRED_SAMPLES 3U

static bool threshold_is_valid(const control_config *parameters, int value)
{
    return value >= parameters->minimum_threshold && value <= parameters->maximum_threshold;
}

control_result_code control_initialize(control_state *state,
    const control_config *parameters,
    uint64_t boot_id,
    uint64_t now_ms,
    bool saved_data_valid,
    int threshold)
{
    if (state == NULL || parameters == NULL || boot_id == 0 ||
        parameters->warmup_time_ms == 0 || parameters->data_timeout_ms == 0 ||
        parameters->edit_timeout_ms == 0 || parameters->save_timeout_ms == 0 || parameters->required_sample_count == 0 ||
        parameters->minimum_threshold < 1 || parameters->maximum_threshold > 4095 ||
        parameters->maximum_threshold < parameters->minimum_threshold || parameters->threshold_hysteresis <= 0 ||
        parameters->threshold_hysteresis > parameters->minimum_threshold ||
        !threshold_is_valid(parameters, parameters->initial_draft_threshold)) {
        return CONTROL_INVALID;
    }
    control_config copy = *parameters;
    memset(state, 0, sizeof(*state));
    state->config = copy;
    state->initialized = true;
    state->boot_id = boot_id;
    state->boot_time_ms = now_ms;
    state->current_time_ms = now_ms;
    state->mode = CONTROL_MODE_AUTO;
    state->sensor_status = SENSOR_WARMING_UP;
    state->edit_status = EDIT_IDLE;
    state->configuration_valid = saved_data_valid && threshold_is_valid(&copy, threshold);
    state->threshold = state->configuration_valid ? threshold : 0;
    return CONTROL_OK;
}

static void evaluate_auto_mode(control_state *state, bool just_entered)
{
    if (state->mode != CONTROL_MODE_AUTO) {
        return;
    }

    if (just_entered || state->sensor_status != SENSOR_READY ||
        !state->configuration_valid) {
        state->relay_on = false;
    }

    if (state->sensor_status == SENSOR_READY && state->configuration_valid) {
        if (state->sample >= state->threshold) {
            state->relay_on = true;
        } else if (state->sample <= state->threshold - state->config.threshold_hysteresis) {
            state->relay_on = false;
        }
    }
}

static void set_notification(
    control_result *result,
    control_notification_type notification,
    const control_state *state)
{
    result->notification = notification;
    result->notification_sample = state->sample;
    result->notification_threshold = state->threshold;
}

/* Cap nhat chot canh bao chi tu mau moi da hop le va Ready. */
static void evaluate_gas_alarm(control_state *state, control_result *result)
{
    if (!state->configuration_valid || !state->has_sample ||
        state->sensor_status != SENSOR_READY) {
        state->consecutive_recovery_sample_count = 0;
        return;
    }

    if (!state->gas_alarm_active) {
        state->consecutive_recovery_sample_count = 0;
        if (state->sample >= state->threshold) {
            state->gas_alarm_active = true;
            set_notification(result, CONTROL_NOTIFICATION_GAS_ALARM, state);
        }
        return;
    }

    const int recovery_threshold = state->threshold - state->config.threshold_hysteresis;
    if (state->sample > recovery_threshold) {
        state->consecutive_recovery_sample_count = 0;
        return;
    }

    if (state->consecutive_recovery_sample_count < GAS_RECOVERY_REQUIRED_SAMPLES) {
        ++state->consecutive_recovery_sample_count;
    }

    if (state->consecutive_recovery_sample_count == GAS_RECOVERY_REQUIRED_SAMPLES) {
        state->gas_alarm_active = false;
        state->consecutive_recovery_sample_count = 0;
        set_notification(result, CONTROL_NOTIFICATION_GAS_RECOVERY, state);
    }
}

/* Nguong moi da duoc luu: mau Ready hien tai co the kich hoat canh bao moi. */
static void evaluate_saved_threshold(control_state *state, control_result *result)
{
    state->consecutive_recovery_sample_count = 0;

    if (state->configuration_valid && state->has_sample &&
        state->sensor_status == SENSOR_READY && !state->gas_alarm_active &&
        state->sample >= state->threshold) {
        state->gas_alarm_active = true;
        set_notification(result, CONTROL_NOTIFICATION_GAS_ALARM, state);
    }
}

static void mark_sensor_error(control_state *state)
{
    state->sensor_status = SENSOR_ERROR;
    state->consecutive_sample_count = 0;
    state->consecutive_recovery_sample_count = 0;
    evaluate_auto_mode(state, false);
}

static void check_timeouts(control_state *state)
{
    uint64_t last_mark =
        state->has_sample ? state->sample_time_ms : state->boot_time_ms;
    if (state->current_time_ms - last_mark >= state->config.data_timeout_ms) {
        mark_sensor_error(state);
    }

    if (state->edit_status == EDIT_ACTIVE &&
        state->current_time_ms - state->last_edit_time_ms >=
            state->config.edit_timeout_ms) {
        state->edit_status = EDIT_IDLE;
        state->draft = 0;
    }

    if (state->edit_status == EDIT_SAVING &&
        state->current_time_ms - state->save_start_time_ms >=
            state->config.save_timeout_ms) {
        /* Có thể đã ghi thành công: giữ giao dịch này chờ xác minh. */
        state->save_result_unknown = true;
    }
}

static control_result_code accept_sample(
    control_state *state,
    const control_event *event,
    control_result *result_event)
{
    if (event->sequence == 0 || event->sequence <= state->sample_sequence ||
        event->sample_time_ms < state->boot_time_ms ||
        event->sample_time_ms > state->current_time_ms ||
        (state->sample_sequence != 0 && event->sample_time_ms <= state->sample_time_ms)) {
        return CONTROL_STALE;
    }
    bool sample_gap =
        state->sample_sequence != 0 && event->sequence - state->sample_sequence != 1;
    state->sample_sequence = event->sequence;
    state->sample_time_ms = event->sample_time_ms;
    state->has_sample = true;
    if (!event->is_valid || event->value < 0 || event->value > 4095 ||
        state->current_time_ms - event->sample_time_ms >= state->config.data_timeout_ms) {
        mark_sensor_error(state);
        return CONTROL_INVALID;
    }

    if (sample_gap) {
        mark_sensor_error(state);
    }
    bool was_ready = state->sensor_status == SENSOR_READY;
    state->sample = event->value;
    if (state->consecutive_sample_count < state->config.required_sample_count) {
        ++state->consecutive_sample_count;
    }

    if (state->current_time_ms - state->boot_time_ms >=
            state->config.warmup_time_ms &&
        state->consecutive_sample_count >= state->config.required_sample_count) {
        state->sensor_status = SENSOR_READY;
    }
    evaluate_auto_mode(
        state, !was_ready && state->sensor_status == SENSOR_READY);
    evaluate_gas_alarm(state, result_event);
    return CONTROL_OK;
}

static control_result_code validate_edit_session(
    const control_state *state, const control_event *event)
{
    if (state->edit_status == EDIT_SAVING) {
        return CONTROL_BUSY;
    }

    if (state->edit_status != EDIT_ACTIVE) {
        return CONTROL_NO_EDIT_SESSION;
    }

    if (event->edit_session_id != state->edit_session_id ||
        event->expected_revision != state->draft_revision) {
        return CONTROL_STALE;
    }

    return CONTROL_OK;
}

static control_result_code apply_event(
    control_state *state, const control_event *event, control_result *result_event)
{
    control_result_code result_code;
    if (event->boot_id != state->boot_id) {
        return CONTROL_STALE;
    }
    switch (event->type) {
    case CONTROL_EVENT_CHECK_TIMEOUTS:
        return CONTROL_OK;
    case CONTROL_EVENT_NEW_SAMPLE:
        return accept_sample(state, event, result_event);
    case CONTROL_EVENT_SET_MODE:
        if (event->value != CONTROL_MODE_AUTO && event->value != CONTROL_MODE_MANUAL)
            return CONTROL_INVALID;
        if (state->edit_status != EDIT_IDLE)
            return CONTROL_BUSY;
        if (state->mode != (control_mode)event->value) {
            state->mode = (control_mode)event->value;
            evaluate_auto_mode(state, true);
        }

        return CONTROL_OK;
    case CONTROL_EVENT_SET_RELAY:
        if (event->value != 0 && event->value != 1)
            return CONTROL_INVALID;
        if (state->edit_status != EDIT_IDLE)
            return CONTROL_BUSY;
        if (state->mode != CONTROL_MODE_MANUAL)
            return CONTROL_WRONG_MODE;
        state->relay_on = event->value != 0;
        return CONTROL_OK;
    case CONTROL_EVENT_BEGIN_EDIT:
        if (state->edit_status != EDIT_IDLE)
            return CONTROL_BUSY;
        if (state->edit_session_id == UINT64_MAX)
            return CONTROL_ID_EXHAUSTED;
        ++state->edit_session_id;
        state->draft_revision = 1;
        state->draft = state->configuration_valid
                                   ? state->threshold
                                   : state->config.initial_draft_threshold;
        state->last_edit_time_ms = state->current_time_ms;
        state->edit_status = EDIT_ACTIVE;
        return CONTROL_OK;
    case CONTROL_EVENT_SET_DRAFT:
        result_code = validate_edit_session(state, event);
        if (result_code != CONTROL_OK)
            return result_code;
        if (!threshold_is_valid(&state->config, event->value))
            return CONTROL_INVALID;
        if (event->value != state->draft) {
            if (state->draft_revision == UINT64_MAX)
                return CONTROL_ID_EXHAUSTED;
            state->draft = event->value;
            ++state->draft_revision;
        }
        state->last_edit_time_ms = state->current_time_ms;
        return CONTROL_OK;
    case CONTROL_EVENT_SAVE_DRAFT:
        result_code = validate_edit_session(state, event);
        if (result_code != CONTROL_OK)
            return result_code;
        if (state->save_id == UINT64_MAX)
            return CONTROL_ID_EXHAUSTED;
        ++state->save_id;
        state->edit_status = EDIT_SAVING;
        state->save_start_time_ms = state->current_time_ms;
        state->save_result_unknown = false;
        result_event->needs_save = true;
        result_event->save_id = state->save_id;
        result_event->threshold_to_save = state->draft;
        return CONTROL_OK;
    case CONTROL_EVENT_SAVE_OK:
    case CONTROL_EVENT_SAVE_FAILED:
        if (state->edit_status != EDIT_SAVING ||
            event->save_id != state->save_id)
            return CONTROL_STALE;
        state->save_result_unknown = false;
        if (event->type == CONTROL_EVENT_SAVE_FAILED) {
            state->edit_status = EDIT_ACTIVE;
            state->last_edit_time_ms = state->current_time_ms;
            return CONTROL_STORAGE_ERROR;
        }
        state->threshold = state->draft;
        state->configuration_valid = true;
        state->edit_status = EDIT_IDLE;
        state->draft = 0;
        evaluate_auto_mode(state, false);
        evaluate_saved_threshold(state, result_event);
        return CONTROL_OK;
    default:
        return CONTROL_INVALID;
    }
}

control_result_code control_process_event(
    control_state *state, const control_event *event, uint64_t now_ms, control_result *result_event)
{
    if (result_event == NULL)
        return CONTROL_INVALID;
    memset(result_event, 0, sizeof(*result_event));
    result_event->result_code = CONTROL_INVALID;
    if (state == NULL || event == NULL || !state->initialized)
        return CONTROL_INVALID;
    result_event->event = event->type;
    result_event->event_time_ms = now_ms;
    bool previous_relay = state->relay_on;
    if (now_ms < state->current_time_ms) {
        result_event->result_code = CONTROL_TIME_ERROR;
    } else {
        state->current_time_ms = now_ms;

        /* Lệnh bị từ chối vẫn phải cho phép phát hiện dữ liệu cảm biến hết hạn. */
        check_timeouts(state);
        result_event->result_code = apply_event(state, event, result_event);
    }
    result_event->relay_changed = previous_relay != state->relay_on;
    result_event->relay_on = state->relay_on;
    result_event->mode = state->mode;
    result_event->sensor_status = state->sensor_status;
    result_event->edit_status = state->edit_status;
    result_event->save_result_unknown = state->save_result_unknown;
    return result_event->result_code;
}

static uint64_t calculate_deadline(uint64_t start, uint64_t khoang_thoi_gian)
{
    return khoang_thoi_gian > UINT64_MAX - start ? UINT64_MAX : start + khoang_thoi_gian;
}

uint64_t control_next_deadline(const control_state *state)
{
    if (state == NULL || !state->initialized)
        return UINT64_MAX;
    uint64_t next_deadline = UINT64_MAX;
    if (state->sensor_status != SENSOR_ERROR || state->consecutive_sample_count > 0) {
        next_deadline = calculate_deadline(
            state->has_sample ? state->sample_time_ms : state->boot_time_ms,
            state->config.data_timeout_ms);
    }
    uint64_t save_deadline = UINT64_MAX;
    if (state->edit_status == EDIT_ACTIVE)
        save_deadline =
            calculate_deadline(state->last_edit_time_ms, state->config.edit_timeout_ms);
    if (state->edit_status == EDIT_SAVING && !state->save_result_unknown)
        save_deadline = calculate_deadline(state->save_start_time_ms, state->config.save_timeout_ms);
    return save_deadline < next_deadline ? save_deadline : next_deadline;
}
