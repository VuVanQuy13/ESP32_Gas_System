#ifndef TELEGRAM_NOTIFY_TYPES_H
#define TELEGRAM_NOTIFY_TYPES_H

typedef enum {
    TELEGRAM_NOTIFY_EVENT_ALARM = 0,
    TELEGRAM_NOTIFY_EVENT_RECOVERY
} telegram_notify_event_type;

typedef struct {
    telegram_notify_event_type type;
    int adc_value;
    int threshold;
} telegram_notify_event;

#endif
