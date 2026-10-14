#pragma once
#include "esp_err.h"
/* Startup gọi một lần sau NVS; NOT_FOUND nếu chưa có thông tin Wi-Fi cục bộ. */
esp_err_t network_start(void);
