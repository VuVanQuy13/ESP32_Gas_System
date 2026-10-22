#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    int d0_level;
    bool flame_detected;
} flame_d0_reading;

esp_err_t flame_d0_initialize(void);
esp_err_t flame_d0_read(flame_d0_reading *reading);
