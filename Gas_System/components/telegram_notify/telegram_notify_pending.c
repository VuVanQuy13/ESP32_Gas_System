#include "telegram_notify_pending.h"

#include <stddef.h>

bool telegram_notify_pending_handle_busy(
    telegram_notify_pending_state *state,
    const telegram_notify_event *event)
{
    if (state == NULL || event == NULL) {
        return false;
    }

    if (event->transitions & TELEGRAM_NOTIFY_FIRE_DETECTED) {
        state->fire_clear_pending = false;
    }

    if (!(event->transitions & TELEGRAM_NOTIFY_FIRE_CLEARED)) {
        return false;
    }

    state->fire_clear_event = *event;
    state->fire_clear_pending = true;
    return true;
}

bool telegram_notify_pending_take(
    telegram_notify_pending_state *state,
    telegram_notify_event *event)
{
    if (state == NULL || event == NULL || !state->fire_clear_pending) {
        return false;
    }

    *event = state->fire_clear_event;
    state->fire_clear_pending = false;
    return true;
}
