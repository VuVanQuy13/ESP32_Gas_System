#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

typedef struct {
    int d0_level;
    bool flame_detected;
} flame_d0_reading;

esp_err_t flame_d0_initialize(void);

/* ISR chi danh thuc sensor_task; task se doc lai D0 truoc khi gui event dieu khien. */
esp_err_t flame_d0_enable_interrupts(TaskHandle_t task_to_notify);

esp_err_t flame_d0_read(flame_d0_reading *reading);
