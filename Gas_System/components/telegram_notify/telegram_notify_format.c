#include "telegram_notify_format.h"

#include <stdio.h>

int telegram_notify_format_message(
    const telegram_notify_event *event,
    char *output,
    size_t output_size
)
{
    if (event == NULL || output == NULL || output_size == 0) {
        return -1;
    }

    if (event->adc_value < 0 || event->adc_value > 4095 ||
        event->threshold < 0 || event->threshold > 4095) {
        return -1;
    }

    int written;

    if (event->type == TELEGRAM_NOTIFY_EVENT_ALARM) {
        written = snprintf(
            output,
            output_size,
            "CANH BAO KHI GAS: Khí Gas=%d, nguong=%d.",
            event->adc_value,
            event->threshold
        );
    } else if (event->type == TELEGRAM_NOTIFY_EVENT_RECOVERY) {
        written = snprintf(
            output,
            output_size,
            "KHI GAS ON DINH: Khí Gas=%d, nguong=%d.",
            event->adc_value,
            event->threshold
        );
    } else {
        return -1;
    }

    if (written < 0 || (size_t)written >= output_size) {
        return -1;
    }

    return written;
}
