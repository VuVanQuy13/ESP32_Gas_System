#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    char close_count[2][17];
} lcd_frame;

typedef struct {
    uint8_t rs, rw, enable, backlight, d4, d5, d6, d7;
    bool backlight_active_high;
} lcd_mapping;

typedef struct {
    esp_err_t (*write)(uint8_t data, void *context);
    void (*delay_us)(unsigned us, void *context);
    void *context;
    lcd_mapping mapping;
    bool ready;
} lcd_device;

/* Sơ đồ phổ biến, chưa xác nhận trên backpack thực tế. */
lcd_mapping lcd_default_mapping(void);

/* Các bit phải là tám mặt nạ một bit khác nhau. RW luôn ở mức ghi. */
esp_err_t lcd_initialize(lcd_device *lcd);

/* Hai dòng đúng 16 ký tự ASCII có dấu kết thúc; lỗi giữa chừng buộc init lại. */
esp_err_t lcd_write(lcd_device *lcd, const lcd_frame *frame);
