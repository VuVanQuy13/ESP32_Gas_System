#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    char dong[2][17];
} lcd_khung;

typedef struct {
    uint8_t rs, rw, e, den, d4, d5, d6, d7;
    bool den_muc_cao;
} lcd_so_do;

typedef struct {
    esp_err_t (*ghi)(uint8_t du_lieu, void *ngu_canh);
    void (*cho_us)(unsigned us, void *ngu_canh);
    void *ngu_canh;
    lcd_so_do so_do;
    bool san_sang;
} lcd_thiet_bi;

/* Sơ đồ phổ biến, chưa xác nhận trên backpack thực tế. */
lcd_so_do lcd_so_do_mac_dinh(void);

/* Các bit phải là tám mặt nạ một bit khác nhau. RW luôn ở mức ghi. */
esp_err_t lcd_khoi_tao(lcd_thiet_bi *lcd);

/* Hai dòng đúng 16 ký tự ASCII có dấu kết thúc; lỗi giữa chừng buộc init lại. */
esp_err_t lcd_viet(lcd_thiet_bi *lcd, const lcd_khung *khung);
