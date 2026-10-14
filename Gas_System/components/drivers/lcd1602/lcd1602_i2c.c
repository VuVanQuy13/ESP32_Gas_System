#include "lcd1602_i2c.h"
#include "board_pins.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static esp_err_t find_address(lcd_i2c *lcd)
{
    unsigned device_count = 0;
    uint8_t found_address = 0;
    for (unsigned i = 0; i < 16; ++i) {
        uint8_t address = (uint8_t)(i < 8 ? 0x20 + i : 0x38 + i - 8);
        esp_err_t error = i2c_master_probe(lcd->bus, address, 20);
        if (error == ESP_OK) {
            ++device_count;
            found_address = address;
        } else if (error != ESP_ERR_NOT_FOUND)
            return error;
    }

    if (!device_count)
        return ESP_ERR_NOT_FOUND;
    if (device_count != 1) {
        ESP_LOGE("lcd", "su_kien=nhieu_dia_chi so=%u", device_count);
        return ESP_ERR_INVALID_STATE;
    }
    lcd->address = found_address;
    ESP_LOGI("lcd", "su_kien=tim_dia_chi dia_chi=0x%02x", found_address);
    return ESP_OK;
}

static esp_err_t send_byte(uint8_t snapshot, void *ctx)
{
    lcd_i2c *lcd = ctx;
    return i2c_master_transmit(lcd->device, &snapshot, 1, 20);
}

static void delay_us(unsigned us, void *ctx)
{
    (void)ctx;
    if (us >= 1000) {
        /* Thêm một tick để thời gian thực không ngắn hơn khoảng yêu cầu. */
        vTaskDelay(pdMS_TO_TICKS((us + 999) / 1000) + 1);
    } else
        esp_rom_delay_us(us);
}

esp_err_t lcd_i2c_output(const lcd_frame *frame, void *ctx)
{
    lcd_i2c *lcd = ctx;
    if (!lcd || !frame ||
        (!lcd->auto_find_address && !((lcd->address >= 0x20 && lcd->address <= 0x27) ||
                                   (lcd->address >= 0x38 && lcd->address <= 0x3f))))
        return ESP_ERR_INVALID_ARG;
    esp_err_t error;
    if (!lcd->bus) {
        const i2c_master_bus_config_t config = {
            .i2c_port = 0,
            .sda_io_num = CONFIG_GAS_GPIO_LCD_SDA,
            .scl_io_num = CONFIG_GAS_GPIO_LCD_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7
        };
        error = i2c_new_master_bus(&config, &lcd->bus);
        if (error != ESP_OK)
            return error;
    }

    if (!lcd->device) {
        if (lcd->auto_find_address) {
            error = find_address(lcd);
            if (error != ESP_OK)
                return error;
        }
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = lcd->address,
            .scl_speed_hz = 100000
        };
        error = i2c_master_bus_add_device(lcd->bus, &config, &lcd->device);
        if (error != ESP_OK)
            return error;
        lcd->lcd =
            (lcd_device){
                .write = send_byte,
                .delay_us = delay_us,
                .context = lcd,
                .mapping = lcd->mapping
            };
    }

    if (!lcd->lcd.ready) {
        error = lcd_initialize(&lcd->lcd);
        if (error != ESP_OK)
            return error;
    }

    return lcd_write(&lcd->lcd, frame);
}
