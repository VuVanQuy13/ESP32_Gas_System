#pragma once
#include "control_task.h"
/* Tạo task quét nút sau button_initialize(); service phải tồn tại đến khi reset. */
esp_err_t button_task_start(control_service_t *service);
