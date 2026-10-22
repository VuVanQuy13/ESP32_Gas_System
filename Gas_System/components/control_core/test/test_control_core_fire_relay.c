#include "control_core.h"

#include <assert.h>
#include <stdio.h>

static control_config make_config(int hysteresis, uint64_t warmup_ms)
{
    return (control_config){
        .warmup_time_ms = warmup_ms,
        .data_timeout_ms = 500,
        .edit_timeout_ms = 30000,
        .save_timeout_ms = 2000,
        .required_sample_count = 3,
        .minimum_threshold = 100,
        .maximum_threshold = 4000,
        .threshold_hysteresis = hysteresis,
        .initial_draft_threshold = 600,
    };
}

static void initialize(control_state *state, int hysteresis, uint64_t warmup_ms, bool saved)
{
    control_config config = make_config(hysteresis, warmup_ms);
    assert(control_initialize(state, &config, 1, 0, saved, 600) == CONTROL_OK);
}

static control_result send_sample(
    control_state *state,
    uint64_t sequence,
    uint64_t time_ms,
    bool gas_valid,
    int adc,
    bool fire_read_valid,
    bool fire_detected)
{
    control_event event = {
        .type = CONTROL_EVENT_NEW_SAMPLE,
        .boot_id = 1,
        .sequence = sequence,
        .sample_time_ms = time_ms,
        .is_valid = gas_valid,
        .fire_read_valid = fire_read_valid,
        .fire_detected = fire_detected,
        .value = adc,
    };
    control_result result;
    (void)control_process_event(state, &event, time_ms, &result);
    return result;
}

static control_result send_mode(control_state *state, control_mode mode, uint64_t time_ms)
{
    control_event event = {
        .type = CONTROL_EVENT_SET_MODE,
        .boot_id = 1,
        .value = mode,
    };
    control_result result;
    assert(control_process_event(state, &event, time_ms, &result) == CONTROL_OK);
    return result;
}

static control_result set_manual_relay(control_state *state, bool on, uint64_t time_ms)
{
    control_event event = {
        .type = CONTROL_EVENT_SET_RELAY,
        .boot_id = 1,
        .value = on,
    };
    control_result result;
    assert(control_process_event(state, &event, time_ms, &result) == CONTROL_OK);
    return result;
}

static void test_gas_hysteresis_uses_configured_value(void)
{
    control_state state;
    initialize(&state, 30, 100, true);
    send_sample(&state, 1, 100, true, 400, true, false);
    send_sample(&state, 2, 200, true, 400, true, false);
    control_result result = send_sample(&state, 3, 300, true, 400, true, false);
    assert(result.sensor_status == SENSOR_READY);
    assert(!result.relay_on);

    result = send_sample(&state, 4, 400, true, 600, true, false);
    assert(result.relay_on);
    assert(result.notifications & CONTROL_NOTIFICATION_GAS_ALARM);

    result = send_sample(&state, 5, 500, true, 571, true, false);
    assert(result.relay_on);
    result = send_sample(&state, 6, 600, true, 570, true, false);
    assert(!result.relay_on);

    send_sample(&state, 7, 700, true, 570, true, false);
    result = send_sample(&state, 8, 800, true, 570, true, false);
    assert(result.notifications & CONTROL_NOTIFICATION_GAS_RECOVERY);
    result = send_sample(&state, 9, 900, true, 600, true, false);
    assert(result.notifications & CONTROL_NOTIFICATION_GAS_ALARM);
    send_sample(&state, 10, 1000, true, 570, true, false);
    send_sample(&state, 11, 1100, true, 570, true, false);
    result = send_sample(&state, 12, 1200, true, 570, true, false);
    assert(result.notifications & CONTROL_NOTIFICATION_GAS_RECOVERY);
}

static void test_fire_turns_on_during_gas_warmup_without_threshold(void)
{
    control_state state;
    initialize(&state, 50, 5000, false);

    control_result result = send_sample(&state, 1, 100, false, 0, true, true);
    assert(result.sensor_status == SENSOR_ERROR);
    assert(result.relay_on);
    assert(result.notifications & CONTROL_NOTIFICATION_FIRE_DETECTED);
    assert(!state.configuration_valid);
}

static void test_fire_overrides_low_gas_and_clears_to_gas_policy(void)
{
    control_state state;
    initialize(&state, 50, 100, true);
    send_sample(&state, 1, 100, true, 400, true, false);
    send_sample(&state, 2, 200, true, 400, true, false);
    send_sample(&state, 3, 300, true, 400, true, false);

    control_result result = send_sample(&state, 4, 400, true, 400, true, true);
    assert(result.relay_on);
    assert(result.notifications & CONTROL_NOTIFICATION_FIRE_DETECTED);

    result = send_sample(&state, 5, 500, true, 400, true, false);
    assert(!result.relay_on);
    assert((result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED) == 0);

    result = send_sample(&state, 6, 3499, true, 400, true, false);
    assert((result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED) == 0);

    result = send_sample(&state, 7, 3500, true, 400, true, false);
    assert(result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED);
}

static void test_gas_alarm_and_fire_detection_are_coalescible(void)
{
    control_state state;
    initialize(&state, 50, 100, true);
    send_sample(&state, 1, 100, true, 400, true, false);
    send_sample(&state, 2, 200, true, 400, true, false);
    send_sample(&state, 3, 300, true, 400, true, false);

    control_result result = send_sample(&state, 4, 400, true, 600, true, true);
    assert(result.relay_on);
    assert((result.notifications & CONTROL_NOTIFICATION_GAS_ALARM) != 0);
    assert((result.notifications & CONTROL_NOTIFICATION_FIRE_DETECTED) != 0);
    assert(result.notification_sample == 600);
    assert(result.notification_threshold == 600);

    result = send_sample(&state, 5, 500, true, 590, true, false);
    assert((result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED) == 0);
    send_sample(&state, 6, 900, true, 590, true, false);
    send_sample(&state, 7, 1300, true, 590, true, false);
    send_sample(&state, 8, 1700, true, 590, true, false);
    send_sample(&state, 9, 2100, true, 590, true, false);
    send_sample(&state, 10, 2500, true, 590, true, false);
    send_sample(&state, 11, 2900, true, 590, true, false);
    send_sample(&state, 12, 3100, true, 550, true, false);
    send_sample(&state, 13, 3300, true, 550, true, false);
    result = send_sample(&state, 14, 3500, true, 550, true, false);
    assert(result.notifications & CONTROL_NOTIFICATION_GAS_RECOVERY);
    assert(result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED);
    assert(!result.relay_on);
}

static void test_fire_read_error_is_not_a_fire_clear_notification(void)
{
    control_state state;
    initialize(&state, 50, 100, true);
    assert(!state.fire_read_valid);
    send_sample(&state, 1, 100, true, 400, true, false);
    assert(state.fire_read_valid);
    send_sample(&state, 2, 200, true, 400, true, false);
    send_sample(&state, 3, 300, true, 400, true, false);

    control_result result = send_sample(&state, 4, 400, true, 400, true, true);
    assert(result.relay_on);
    assert(state.fire_notification_active);

    result = send_sample(&state, 5, 500, true, 400, true, false);
    assert(!result.relay_on);
    assert(result.notifications == CONTROL_NOTIFICATION_NONE);
    assert(state.fire_notification_active);

    result = send_sample(&state, 6, 600, true, 400, false, false);
    assert((result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED) == 0);
    assert(!state.fire_read_valid);
    result = send_sample(&state, 7, 700, true, 400, true, false);
    assert(state.fire_read_valid);
    result = send_sample(&state, 8, 3699, true, 400, true, false);
    assert((result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED) == 0);
    result = send_sample(&state, 9, 3700, true, 400, true, false);
    assert(result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED);
    assert(!state.fire_notification_active);
}

static void test_manual_mode_keeps_user_authority(void)
{
    control_state state;
    initialize(&state, 50, 100, true);
    send_mode(&state, CONTROL_MODE_MANUAL, 10);

    control_result result = send_sample(&state, 1, 100, false, 0, true, true);
    assert(!result.relay_on);
    assert(result.notifications & CONTROL_NOTIFICATION_FIRE_DETECTED);

    result = set_manual_relay(&state, true, 110);
    assert(result.relay_on);
    result = send_sample(&state, 2, 200, false, 0, true, false);
    assert(result.relay_on);
    assert((result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED) == 0);
    result = send_sample(&state, 3, 3200, false, 0, true, false);
    assert(result.notifications & CONTROL_NOTIFICATION_FIRE_CLEARED);
    result = set_manual_relay(&state, false, 3210);
    assert(!result.relay_on);
}

int main(void)
{
    test_gas_hysteresis_uses_configured_value();
    test_fire_turns_on_during_gas_warmup_without_threshold();
    test_fire_overrides_low_gas_and_clears_to_gas_policy();
    test_gas_alarm_and_fire_detection_are_coalescible();
    test_fire_read_error_is_not_a_fire_clear_notification();
    test_manual_mode_keeps_user_authority();
    puts("PASS: control core gas/fire relay scenarios");
    return 0;
}
