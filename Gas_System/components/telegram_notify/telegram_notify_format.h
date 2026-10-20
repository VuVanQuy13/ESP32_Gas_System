#ifndef TELEGRAM_NOTIFY_FORMAT_H
#define TELEGRAM_NOTIFY_FORMAT_H

#include <stddef.h>

#include "telegram_notify_types.h"

#define TELEGRAM_NOTIFY_MESSAGE_MAX 128

/* Ham thuan C: kiem tra va tao noi dung tin nhan ADC tho. */
int telegram_notify_format_message(
    const telegram_notify_event *event,
    char *output,
    size_t output_size
);

#endif
