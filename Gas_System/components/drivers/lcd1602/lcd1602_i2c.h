#pragma once
#include "lcd1602.h"
#include "driver/i2c_master.h"

typedef struct {
    uint8_t address;
    bool auto_find_address; /* Chỉ chọn khi có đúng một địa chỉ PCF phản hồi. */
    lcd_mapping mapping;
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    lcd_device lcd;
} lcd_i2c;

/* Khởi tạo struct bằng 0 rồi đặt địa chỉ/sơ đồ; chỉ display_task truy cập.
 * Bus/device được giữ đến reset, kể cả lỗi để retry không tạo tài nguyên chồng. */
esp_err_t lcd_i2c_output(const lcd_frame *frame, void *context);
