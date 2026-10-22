#include "telegram_notify_format.h"
#include "telegram_notify_pending.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_gas_alarm(void)
{
    char output[TELEGRAM_NOTIFY_MESSAGE_MAX];
    telegram_notify_event event = {
        .transitions = TELEGRAM_NOTIFY_GAS_ALARM,
        .has_gas_reading = true,
        .adc_value = 625,
        .threshold = 600,
    };
    assert(telegram_notify_format_message(&event, output, sizeof(output)) > 0);
    assert(strstr(output, "CANH BAO KHI GAS") != NULL);
    assert(strstr(output, "ppm=625") != NULL);
}

static void test_fire_detection_and_recovery(void)
{
    char output[TELEGRAM_NOTIFY_MESSAGE_MAX];
    telegram_notify_event event = {
        .transitions = TELEGRAM_NOTIFY_FIRE_DETECTED,
    };
    assert(telegram_notify_format_message(&event, output, sizeof(output)) > 0);
    assert(strstr(output, "Phat hien lua") != NULL);

    event.transitions = TELEGRAM_NOTIFY_FIRE_CLEARED;
    assert(telegram_notify_format_message(&event, output, sizeof(output)) > 0);
    assert(strstr(output, "xac nhan het lua") != NULL);
}

static void test_simultaneous_gas_and_fire_are_one_message(void)
{
    char output[TELEGRAM_NOTIFY_MESSAGE_MAX];
    telegram_notify_event event = {
        .transitions = TELEGRAM_NOTIFY_GAS_ALARM | TELEGRAM_NOTIFY_FIRE_DETECTED,
        .has_gas_reading = true,
        .adc_value = 700,
        .threshold = 600,
    };
    assert(telegram_notify_format_message(&event, output, sizeof(output)) > 0);
    assert(strstr(output, "CANH BAO KHI GAS") != NULL);
    assert(strstr(output, "CANH BAO LUA") != NULL);
    assert(strchr(output, '\n') != NULL);
}

static void test_validation_and_capacity(void)
{
    char output[TELEGRAM_NOTIFY_MESSAGE_MAX];
    telegram_notify_event event = { 0 };
    assert(telegram_notify_format_message(&event, output, sizeof(output)) < 0);

    event.transitions = TELEGRAM_NOTIFY_GAS_ALARM | TELEGRAM_NOTIFY_GAS_RECOVERY;
    event.has_gas_reading = true;
    event.adc_value = 500;
    event.threshold = 600;
    assert(telegram_notify_format_message(&event, output, sizeof(output)) < 0);

    event.transitions = TELEGRAM_NOTIFY_FIRE_DETECTED;
    event.has_gas_reading = false;
    assert(telegram_notify_format_message(&event, output, 8) < 0);
}

static void test_pending_fire_clear_is_held_and_new_fire_cancels_it(void)
{
    telegram_notify_pending_state state = { 0 };
    telegram_notify_event fire_clear = {
        .transitions = TELEGRAM_NOTIFY_FIRE_CLEARED,
    };
    telegram_notify_event taken = { 0 };

    assert(telegram_notify_pending_handle_busy(&state, &fire_clear));
    assert(state.fire_clear_pending);
    assert(telegram_notify_pending_take(&state, &taken));
    assert(taken.transitions == TELEGRAM_NOTIFY_FIRE_CLEARED);
    assert(!state.fire_clear_pending);

    assert(telegram_notify_pending_handle_busy(&state, &fire_clear));
    telegram_notify_event fire_detected = {
        .transitions = TELEGRAM_NOTIFY_FIRE_DETECTED,
    };
    assert(!telegram_notify_pending_handle_busy(&state, &fire_detected));
    assert(!state.fire_clear_pending);

    telegram_notify_event gas_alarm = {
        .transitions = TELEGRAM_NOTIFY_GAS_ALARM,
        .has_gas_reading = true,
        .adc_value = 700,
        .threshold = 600,
    };
    assert(!telegram_notify_pending_handle_busy(&state, &gas_alarm));
    assert(!state.fire_clear_pending);
}

int main(void)
{
    test_gas_alarm();
    test_fire_detection_and_recovery();
    test_simultaneous_gas_and_fire_are_one_message();
    test_validation_and_capacity();
    test_pending_fire_clear_is_held_and_new_fire_cancels_it();
    puts("PASS: Telegram gas/fire message formatting");
    return 0;
}
