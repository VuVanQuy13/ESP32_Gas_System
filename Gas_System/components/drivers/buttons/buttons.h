#pragma once
#include <stdint.h>
#include "esp_err.h"
esp_err_t nut_khoi_tao(void);

/* Bit 0..3: ON/+, OFF/-, MODE, EDIT/SAVE; bit 1 nghĩa đang nhấn. */
esp_err_t nut_doc(uint8_t *mat_na);
