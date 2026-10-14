#pragma once
#include <stdint.h>
#include "esp_err.h"
esp_err_t button_initialize(void);

/* Bit 0..3: ON/+, OFF/-, MODE, EDIT/SAVE; bit 1 nghĩa đang nhấn. */
esp_err_t button_read(uint8_t *mask);
