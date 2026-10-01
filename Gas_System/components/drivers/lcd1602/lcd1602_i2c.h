#pragma once
#include "lcd1602.h"
#include "driver/i2c_master.h"

typedef struct {
    uint8_t dia_chi;
    bool tu_tim_dia_chi; /* Chỉ chọn khi có đúng một địa chỉ PCF phản hồi. */
    lcd_so_do so_do;
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t thiet_bi;
    lcd_thiet_bi lcd;
} lcd_i2c;

/* Khởi tạo struct bằng 0 rồi đặt địa chỉ/sơ đồ; chỉ display_task truy cập.
 * Bus/device được giữ đến reset, kể cả lỗi để retry không tạo tài nguyên chồng. */
esp_err_t lcd_i2c_xuat(const lcd_khung *khung, void *ngu_canh);
