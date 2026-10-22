#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

esp_err_t button_initialize(void);

/* ISR chi danh thuc task; button_task van phai chong rung truoc khi gui lenh. */
esp_err_t button_enable_interrupts(TaskHandle_t task_to_notify);

/* Bit 0..3: ON/+, OFF/-, MODE, EDIT/SAVE; bit 1 nghia la dang nhan. */
esp_err_t button_read(uint8_t *mask);
