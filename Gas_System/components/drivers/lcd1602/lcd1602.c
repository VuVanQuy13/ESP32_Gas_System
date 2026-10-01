#include "lcd1602.h"
#include <stddef.h>

lcd_so_do lcd_so_do_mac_dinh(void)
{
    /* Đường nối sẵn trên backpack: P0=RS, P1=RW, P2=E, P3=BL,
     * P4..P7=D4..D7. Đây là mặt nạ bit, không phải GPIO ESP32.
     * Nếu thay loại backpack có đường nối khác, sửa mapping tại đây. */
    return (lcd_so_do){
        .rs = 1,
        .rw = 2,
        .e = 4,
        .den = 8,
        .d4 = 16,
        .d5 = 32,
        .d6 = 64,
        .d7 = 128,
        .den_muc_cao = true
    };
}

static bool hop_le(const lcd_thiet_bi *l)
{
    if (!l || !l->ghi || !l->cho_us)
        return false;
    uint8_t bit[] = { l->so_do.rs,
        l->so_do.rw,
        l->so_do.e,
        l->so_do.den,
        l->so_do.d4,
        l->so_do.d5,
        l->so_do.d6,
        l->so_do.d7 };
    unsigned da_co = 0;
    for (unsigned i = 0; i < 8; ++i) {
        if (!bit[i] || (bit[i] & (bit[i] - 1)) || (da_co & bit[i]))
            return false;
        da_co |= bit[i];
    }

    return true;
}

static esp_err_t ghi(lcd_thiet_bi *l, uint8_t b)
{
    esp_err_t loi = l->ghi(b, l->ngu_canh);
    if (loi != ESP_OK)
        l->san_sang = false;
    return loi;
}

static esp_err_t nibble(lcd_thiet_bi *l, uint8_t n, bool du_lieu)
{
    uint8_t b = l->so_do.den_muc_cao ? l->so_do.den : 0;
    if (du_lieu)
        b |= l->so_do.rs;
    if (n & 1)
        b |= l->so_do.d4;
    if (n & 2)
        b |= l->so_do.d5;
    if (n & 4)
        b |= l->so_do.d6;
    if (n & 8)
        b |= l->so_do.d7;
    esp_err_t loi = ghi(l, b); /* Đặt data khi E thấp, RW luôn thấp. */
    if (loi != ESP_OK)
        return loi;
    l->cho_us(1, l->ngu_canh);
    loi = ghi(l, b | l->so_do.e);
    if (loi != ESP_OK)
        return loi;
    l->cho_us(1, l->ngu_canh);
    return ghi(l, b);
}

static esp_err_t byte_lcd(lcd_thiet_bi *l, uint8_t b, bool du_lieu)
{
    esp_err_t loi = nibble(l, b >> 4, du_lieu);
    if (loi == ESP_OK)
        loi = nibble(l, b & 15, du_lieu);
    if (loi == ESP_OK)
        l->cho_us(!du_lieu && (b == 1 || b == 2) ? 2000 : 50, l->ngu_canh);
    return loi;
}

esp_err_t lcd_khoi_tao(lcd_thiet_bi *l)
{
    if (!hop_le(l))
        return ESP_ERR_INVALID_ARG;
    l->san_sang = false;
    esp_err_t loi = ghi(l, l->so_do.den_muc_cao ? l->so_do.den : 0);
    if (loi != ESP_OK)
        return loi;
    l->cho_us(50000, l->ngu_canh);
    const uint8_t dau[] = { 3, 3, 3, 2 };
    const unsigned cho[] = { 5000, 200, 200, 200 };
    for (unsigned i = 0; i < 4; ++i) {
        loi = nibble(l, dau[i], false);
        if (loi != ESP_OK)
            return loi;
        l->cho_us(cho[i], l->ngu_canh);
    }
    const uint8_t lenh[] = { 0x28, 0x08, 0x01, 0x06, 0x0c };
    for (unsigned i = 0; i < sizeof(lenh); ++i) {
        loi = byte_lcd(l, lenh[i], false);
        if (loi != ESP_OK)
            return loi;
    }
    l->san_sang = true;
    return ESP_OK;
}

esp_err_t lcd_viet(lcd_thiet_bi *l, const lcd_khung *k)
{
    if (!hop_le(l) || !k)
        return ESP_ERR_INVALID_ARG;
    for (unsigned d = 0; d < 2; ++d) {
        if (k->dong[d][16] != '\0')
            return ESP_ERR_INVALID_ARG;
        for (unsigned i = 0; i < 16; ++i)
            if ((unsigned char)k->dong[d][i] < 32 || (unsigned char)k->dong[d][i] > 126)
                return ESP_ERR_INVALID_ARG;
    }

    if (!l->san_sang)
        return ESP_ERR_INVALID_STATE;
    for (unsigned d = 0; d < 2; ++d) {
        esp_err_t loi = byte_lcd(l, d == 0 ? 0x80 : 0xc0, false);
        if (loi != ESP_OK)
            return loi;
        for (unsigned i = 0; i < 16; ++i) {
            loi = byte_lcd(l, (uint8_t)k->dong[d][i], true);
            if (loi != ESP_OK)
                return loi;
        }
    }

    return ESP_OK;
}
