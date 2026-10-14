#pragma once
#include <stdbool.h>
#include "esp_err.h"
/* Khởi tạo một lần trước khi chạy task. Sau đó chỉ control_task ghi relay. */
esp_err_t relay_initialize(void);
esp_err_t relay_set(bool on);
