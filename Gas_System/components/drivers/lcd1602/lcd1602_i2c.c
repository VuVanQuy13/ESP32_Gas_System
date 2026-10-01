#include "lcd1602_i2c.h"
#include "board_pins.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static esp_err_t tim_dia_chi(lcd_i2c *l)
{
    unsigned so_thiet_bi = 0;
    uint8_t tim_duoc = 0;
    for (unsigned i = 0; i < 16; ++i) {
        uint8_t dia_chi = (uint8_t)(i < 8 ? 0x20 + i : 0x38 + i - 8);
        esp_err_t loi = i2c_master_probe(l->bus, dia_chi, 20);
        if (loi == ESP_OK) {
            ++so_thiet_bi;
            tim_duoc = dia_chi;
        } else if (loi != ESP_ERR_NOT_FOUND)
            return loi;
    }

    if (!so_thiet_bi)
        return ESP_ERR_NOT_FOUND;
    if (so_thiet_bi != 1) {
        ESP_LOGE("lcd", "su_kien=nhieu_dia_chi so=%u", so_thiet_bi);
        return ESP_ERR_INVALID_STATE;
    }
    l->dia_chi = tim_duoc;
    ESP_LOGI("lcd", "su_kien=tim_dia_chi dia_chi=0x%02x", tim_duoc);
    return ESP_OK;
}

static esp_err_t gui_byte(uint8_t b, void *ctx)
{
    lcd_i2c *l = ctx;
    return i2c_master_transmit(l->thiet_bi, &b, 1, 20);
}

static void cho_us(unsigned us, void *ctx)
{
    (void)ctx;
    if (us >= 1000) {
        /* Thêm một tick để thời gian thực không ngắn hơn khoảng yêu cầu. */
        vTaskDelay(pdMS_TO_TICKS((us + 999) / 1000) + 1);
    } else
        esp_rom_delay_us(us);
}

esp_err_t lcd_i2c_xuat(const lcd_khung *khung, void *ctx)
{
    lcd_i2c *l = ctx;
    if (!l || !khung ||
        (!l->tu_tim_dia_chi && !((l->dia_chi >= 0x20 && l->dia_chi <= 0x27) ||
                                   (l->dia_chi >= 0x38 && l->dia_chi <= 0x3f))))
        return ESP_ERR_INVALID_ARG;
    esp_err_t loi;
    if (!l->bus) {
        const i2c_master_bus_config_t c = {
            .i2c_port = 0,
            .sda_io_num = CONFIG_GAS_GPIO_LCD_SDA,
            .scl_io_num = CONFIG_GAS_GPIO_LCD_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7
        };
        loi = i2c_new_master_bus(&c, &l->bus);
        if (loi != ESP_OK)
            return loi;
    }

    if (!l->thiet_bi) {
        if (l->tu_tim_dia_chi) {
            loi = tim_dia_chi(l);
            if (loi != ESP_OK)
                return loi;
        }
        const i2c_device_config_t c = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = l->dia_chi,
            .scl_speed_hz = 100000
        };
        loi = i2c_master_bus_add_device(l->bus, &c, &l->thiet_bi);
        if (loi != ESP_OK)
            return loi;
        l->lcd =
            (lcd_thiet_bi){
                .ghi = gui_byte,
                .cho_us = cho_us,
                .ngu_canh = l,
                .so_do = l->so_do
            };
    }

    if (!l->lcd.san_sang) {
        loi = lcd_khoi_tao(&l->lcd);
        if (loi != ESP_OK)
            return loi;
    }

    return lcd_viet(&l->lcd, khung);
}
