#include "telegram_notify_format.h"

#include <stdio.h>

static bool append_line(char *output, size_t capacity, size_t *length, const char *line)
{
    if (*length > 0) {
        if (*length + 1 >= capacity)
            return false;
        output[(*length)++] = '\n';
        output[*length] = '\0';
    }

    int written = snprintf(output + *length, capacity - *length, "%s", line);
    if (written < 0 || (size_t)written >= capacity - *length)
        return false;

    *length += (size_t)written;
    return true;
}

int telegram_notify_format_message(
    const telegram_notify_event *event,
    char *output,
    size_t output_size)
{
    if (event == NULL || output == NULL || output_size == 0)
        return -1;

    const uint32_t gas_transitions =
        TELEGRAM_NOTIFY_GAS_ALARM | TELEGRAM_NOTIFY_GAS_RECOVERY;
    const uint32_t fire_transitions =
        TELEGRAM_NOTIFY_FIRE_DETECTED | TELEGRAM_NOTIFY_FIRE_CLEARED;
    const bool has_gas_transition = (event->transitions & gas_transitions) != 0;

    if (event->transitions == 0 ||
        (event->transitions & ~TELEGRAM_NOTIFY_ALL_TRANSITIONS) != 0 ||
        ((event->transitions & gas_transitions) == gas_transitions) ||
        ((event->transitions & fire_transitions) == fire_transitions) ||
        event->has_gas_reading != has_gas_transition ||
        (has_gas_transition &&
            (event->adc_value < 0 || event->adc_value > 4095 ||
                event->threshold < 0 || event->threshold > 4095))) {
        return -1;
    }

    output[0] = '\0';
    size_t length = 0;
    char line[96];

    if (event->transitions & TELEGRAM_NOTIFY_GAS_ALARM) {
        int written = snprintf(
            line, sizeof(line), "CANH BAO KHI GAS: ADC tho=%d, nguong=%d.",
            event->adc_value, event->threshold);
        if (written < 0 || (size_t)written >= sizeof(line) ||
            !append_line(output, output_size, &length, line))
            return -1;
    }

    if (event->transitions & TELEGRAM_NOTIFY_GAS_RECOVERY) {
        int written = snprintf(
            line, sizeof(line), "KHI GAS ON DINH: ADC tho=%d, nguong=%d.",
            event->adc_value, event->threshold);
        if (written < 0 || (size_t)written >= sizeof(line) ||
            !append_line(output, output_size, &length, line))
            return -1;
    }

    if (event->transitions & TELEGRAM_NOTIFY_FIRE_DETECTED) {
        if (!append_line(output, output_size, &length, "CANH BAO LUA: Phat hien lua qua."))
            return -1;
    }

    if (event->transitions & TELEGRAM_NOTIFY_FIRE_CLEARED) {
        if (!append_line(output, output_size, &length, "TINH TRANG LUA: da xac nhan het lua."))
            return -1;
    }

    return length > 0 ? (int)length : -1;
}