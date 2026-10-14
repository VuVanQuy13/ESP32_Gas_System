#pragma once
#include "control_task.h"
#include "lcd1602.h"
/* `display_task_t` là bí danh của struct nội bộ được định nghĩa trong display_task.c. */
typedef struct display_task display_task_t;

typedef struct {
    esp_err_t (*output)(const lcd_frame *frame, void *context);
    void *context; /* Tồn tại đến reset, chỉ display_task dùng. */
} display_task_config;

esp_err_t display_task_start(display_task_t **out, control_service_t *service, const display_task_config *config);
