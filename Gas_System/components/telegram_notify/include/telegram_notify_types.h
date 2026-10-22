#ifndef TELEGRAM_NOTIFY_TYPES_H
#define TELEGRAM_NOTIFY_TYPES_H

#include <stdbool.h>
#include <stdint.h>

enum {
    TELEGRAM_NOTIFY_GAS_ALARM = 1U << 0,
    TELEGRAM_NOTIFY_GAS_RECOVERY = 1U << 1,
    TELEGRAM_NOTIFY_FIRE_DETECTED = 1U << 2,
    TELEGRAM_NOTIFY_FIRE_CLEARED = 1U << 3
};

#define TELEGRAM_NOTIFY_ALL_TRANSITIONS \
    (TELEGRAM_NOTIFY_GAS_ALARM | TELEGRAM_NOTIFY_GAS_RECOVERY | \
        TELEGRAM_NOTIFY_FIRE_DETECTED | TELEGRAM_NOTIFY_FIRE_CLEARED)

typedef struct {
    uint32_t transitions;
    bool has_gas_reading;
    int adc_value;
    int threshold;
} telegram_notify_event;

#endif
