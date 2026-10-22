#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Trang thai debounce cua bon nut active-low. */
typedef struct {
    bool initialized;
    uint8_t candidate;
    uint8_t stable;
    uint8_t armed;
    uint64_t change_time_ms[4];
    uint64_t previous_time_ms;
} button_debouncer;

/* Chi phat edge sau 30 ms on dinh; giu nut khong phat lai edge. */
static inline bool button_debounce_update(
    button_debouncer *filter,
    uint8_t mask,
    uint64_t time_ms,
    uint8_t *edges)
{
    if (!filter || !edges || mask > 15 || (filter->initialized && time_ms < filter->previous_time_ms))
        return false;

    *edges = 0;
    if (!filter->initialized) {
        filter->initialized = true;
        filter->candidate = mask;
        filter->stable = mask;
        filter->armed = (uint8_t)(~mask & 15);
        for (unsigned i = 0; i < 4; ++i)
            filter->change_time_ms[i] = time_ms;
    }

    filter->previous_time_ms = time_ms;
    for (unsigned i = 0; i < 4; ++i) {
        uint8_t bit = (uint8_t)(1U << i);
        if ((mask ^ filter->candidate) & bit) {
            filter->candidate ^= bit;
            filter->change_time_ms[i] = time_ms;
        }

        if (((filter->candidate ^ filter->stable) & bit) &&
            time_ms - filter->change_time_ms[i] >= 30) {
            filter->stable ^= bit;
            if (!(filter->stable & bit)) {
                filter->armed |= bit;
            } else {
                if (filter->armed & bit)
                    *edges |= bit;
                filter->armed &= (uint8_t)~bit;
            }
        }
    }

    return true;
}
