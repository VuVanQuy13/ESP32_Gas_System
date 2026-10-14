#include "lcd1602.h"
#include <stddef.h>

lcd_mapping lcd_default_mapping(void)
{
    /* Đường nối sẵn trên backpack: P0=RS, P1=RW, P2=E, P3=BL,
     * P4..P7=D4..D7. Đây là mặt nạ bit, không phải GPIO ESP32.
     * Nếu thay loại backpack có đường nối khác, sửa mapping tại đây. */
    return (lcd_mapping){
        .rs = 1,
        .rw = 2,
        .enable = 4,
        .backlight = 8,
        .d4 = 16,
        .d5 = 32,
        .d6 = 64,
        .d7 = 128,
        .backlight_active_high = true
    };
}

static bool is_valid(const lcd_device *lcd)
{
    if (!lcd || !lcd->write || !lcd->delay_us)
        return false;

    uint8_t bits[] = { 
        lcd->mapping.rs,
        lcd->mapping.rw,
        lcd->mapping.enable,
        lcd->mapping.backlight,
        lcd->mapping.d4,
        lcd->mapping.d5,
        lcd->mapping.d6,
        lcd->mapping.d7 
    };

    unsigned used_bits = 0;
    for (unsigned i = 0; i < 8; ++i) {
        if (!bits[i] || (bits[i] & (bits[i] - 1)) || (used_bits & bits[i]))
            return false;
        used_bits |= bits[i];
    }

    return true;
}

static esp_err_t write(lcd_device *lcd, uint8_t snapshot)
{
    esp_err_t error = lcd->write(snapshot, lcd->context);
    if (error != ESP_OK)
        lcd->ready = false;
    return error;
}

static esp_err_t lcd_write_nibble(lcd_device *lcd, uint8_t length, bool data)
{
    uint8_t snapshot = lcd->mapping.backlight_active_high ? lcd->mapping.backlight : 0;
    if (data)
        snapshot |= lcd->mapping.rs;
    if (length & 1)
        snapshot |= lcd->mapping.d4;
    if (length & 2)
        snapshot |= lcd->mapping.d5;
    if (length & 4)
        snapshot |= lcd->mapping.d6;
    if (length & 8)
        snapshot |= lcd->mapping.d7;

    esp_err_t error = write(lcd, snapshot); /* Đặt data khi E thấp, RW luôn thấp. */
    if (error != ESP_OK)
        return error;

    lcd->delay_us(1, lcd->context);

    error = write(lcd, snapshot | lcd->mapping.enable);
    if (error != ESP_OK)
        return error;
    
    lcd->delay_us(1, lcd->context);
    return write(lcd, snapshot);
}

static esp_err_t lcd_write_byte(lcd_device *lcd, uint8_t snapshot, bool data)
{
    esp_err_t error = lcd_write_nibble(lcd, snapshot >> 4, data);
    if (error == ESP_OK)
        error = lcd_write_nibble(lcd, snapshot & 15, data);
    if (error == ESP_OK)
        lcd->delay_us(!data && (snapshot == 1 || snapshot == 2) ? 2000 : 50, lcd->context);
    return error;
}

esp_err_t lcd_initialize(lcd_device *lcd)
{
    if (!is_valid(lcd))
        return ESP_ERR_INVALID_ARG;
    lcd->ready = false;
    esp_err_t error = write(lcd, lcd->mapping.backlight_active_high ? lcd->mapping.backlight : 0);
    if (error != ESP_OK)
        return error;
    lcd->delay_us(50000, lcd->context);
    const uint8_t startup_nibbles[] = { 3, 3, 3, 2 };
    const unsigned delays[] = { 5000, 200, 200, 200 };
    for (unsigned i = 0; i < 4; ++i) {
        error = lcd_write_nibble(lcd, startup_nibbles[i], false);
        if (error != ESP_OK)
            return error;
        lcd->delay_us(delays[i], lcd->context);
    }
    const uint8_t command[] = { 0x28, 0x08, 0x01, 0x06, 0x0c };
    for (unsigned i = 0; i < sizeof(command); ++i) {
        error = lcd_write_byte(lcd, command[i], false);
        if (error != ESP_OK)
            return error;
    }
    lcd->ready = true;
    return ESP_OK;
}

esp_err_t lcd_write(lcd_device *lcd, const lcd_frame *result)
{
    if (!is_valid(lcd) || !result)
        return ESP_ERR_INVALID_ARG;
    for (unsigned service = 0; service < 2; ++service) {
        if (result->close_count[service][16] != '\0')
            return ESP_ERR_INVALID_ARG;
        for (unsigned i = 0; i < 16; ++i)
            if ((unsigned char)result->close_count[service][i] < 32 || (unsigned char)result->close_count[service][i] > 126)
                return ESP_ERR_INVALID_ARG;
    }

    if (!lcd->ready)
        return ESP_ERR_INVALID_STATE;
    for (unsigned service = 0; service < 2; ++service) {
        esp_err_t error = lcd_write_byte(lcd, service == 0 ? 0x80 : 0xc0, false);
        if (error != ESP_OK)
            return error;
        for (unsigned i = 0; i < 16; ++i) {
            error = lcd_write_byte(lcd, (uint8_t)result->close_count[service][i], true);
            if (error != ESP_OK)
                return error;
        }
    }

    return ESP_OK;
}
