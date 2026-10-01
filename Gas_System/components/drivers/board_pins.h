#pragma once
#include "sdkconfig.h"
/* Chính sách chân cho ESP32 classic/WROOM: tránh flash, UART0 và strapping.
 * Nút cần pull-up; LCD/relay cần output. GPIO34..39 chỉ dành ADC input. */
#define CHAN_IO_HOP_LE(p)                                                                          \
    ((p) == 4 || (p) == 13 || (p) == 14 || ((p) >= 16 && (p) <= 19) || ((p) >= 21 && (p) <= 23) || \
        ((p) >= 25 && (p) <= 27) || (p) == 32 || (p) == 33)
_Static_assert(
    (CONFIG_GAS_GPIO_MQ2 >= 32 && CONFIG_GAS_GPIO_MQ2 <= 36) || CONFIG_GAS_GPIO_MQ2 == 39,
    "GPIO MQ2 khong hop le; xem menu Chan GPIO");
_Static_assert(
    CHAN_IO_HOP_LE(CONFIG_GAS_GPIO_LCD_SDA), "GPIO LCD_SDA khong hop le; xem menu Chan GPIO");
_Static_assert(
    CHAN_IO_HOP_LE(CONFIG_GAS_GPIO_LCD_SCL), "GPIO LCD_SCL khong hop le; xem menu Chan GPIO");
_Static_assert(
    CHAN_IO_HOP_LE(CONFIG_GAS_GPIO_RELAY), "GPIO RELAY khong hop le; xem menu Chan GPIO");
_Static_assert(
    CHAN_IO_HOP_LE(CONFIG_GAS_GPIO_NUT_BAT), "GPIO NUT_BAT khong hop le; xem menu Chan GPIO");
_Static_assert(
    CHAN_IO_HOP_LE(CONFIG_GAS_GPIO_NUT_TAT), "GPIO NUT_TAT khong hop le; xem menu Chan GPIO");
_Static_assert(
    CHAN_IO_HOP_LE(CONFIG_GAS_GPIO_NUT_CHE_DO), "GPIO NUT_CHE_DO khong hop le; xem menu Chan GPIO");
_Static_assert(
    CHAN_IO_HOP_LE(CONFIG_GAS_GPIO_NUT_CHINH), "GPIO NUT_CHINH khong hop le; xem menu Chan GPIO");
_Static_assert(CONFIG_GAS_GPIO_MQ2 != CONFIG_GAS_GPIO_LCD_SDA, "Trung GPIO: MQ2 / LCD_SDA");
_Static_assert(CONFIG_GAS_GPIO_MQ2 != CONFIG_GAS_GPIO_LCD_SCL, "Trung GPIO: MQ2 / LCD_SCL");
_Static_assert(CONFIG_GAS_GPIO_MQ2 != CONFIG_GAS_GPIO_RELAY, "Trung GPIO: MQ2 / RELAY");
_Static_assert(CONFIG_GAS_GPIO_MQ2 != CONFIG_GAS_GPIO_NUT_BAT, "Trung GPIO: MQ2 / NUT_BAT");
_Static_assert(CONFIG_GAS_GPIO_MQ2 != CONFIG_GAS_GPIO_NUT_TAT, "Trung GPIO: MQ2 / NUT_TAT");
_Static_assert(CONFIG_GAS_GPIO_MQ2 != CONFIG_GAS_GPIO_NUT_CHE_DO, "Trung GPIO: MQ2 / NUT_CHE_DO");
_Static_assert(CONFIG_GAS_GPIO_MQ2 != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: MQ2 / NUT_CHINH");
_Static_assert(CONFIG_GAS_GPIO_LCD_SDA != CONFIG_GAS_GPIO_LCD_SCL, "Trung GPIO: LCD_SDA / LCD_SCL");
_Static_assert(CONFIG_GAS_GPIO_LCD_SDA != CONFIG_GAS_GPIO_RELAY, "Trung GPIO: LCD_SDA / RELAY");
_Static_assert(CONFIG_GAS_GPIO_LCD_SDA != CONFIG_GAS_GPIO_NUT_BAT, "Trung GPIO: LCD_SDA / NUT_BAT");
_Static_assert(CONFIG_GAS_GPIO_LCD_SDA != CONFIG_GAS_GPIO_NUT_TAT, "Trung GPIO: LCD_SDA / NUT_TAT");
_Static_assert(
    CONFIG_GAS_GPIO_LCD_SDA != CONFIG_GAS_GPIO_NUT_CHE_DO, "Trung GPIO: LCD_SDA / NUT_CHE_DO");
_Static_assert(
    CONFIG_GAS_GPIO_LCD_SDA != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: LCD_SDA / NUT_CHINH");
_Static_assert(CONFIG_GAS_GPIO_LCD_SCL != CONFIG_GAS_GPIO_RELAY, "Trung GPIO: LCD_SCL / RELAY");
_Static_assert(CONFIG_GAS_GPIO_LCD_SCL != CONFIG_GAS_GPIO_NUT_BAT, "Trung GPIO: LCD_SCL / NUT_BAT");
_Static_assert(CONFIG_GAS_GPIO_LCD_SCL != CONFIG_GAS_GPIO_NUT_TAT, "Trung GPIO: LCD_SCL / NUT_TAT");
_Static_assert(
    CONFIG_GAS_GPIO_LCD_SCL != CONFIG_GAS_GPIO_NUT_CHE_DO, "Trung GPIO: LCD_SCL / NUT_CHE_DO");
_Static_assert(
    CONFIG_GAS_GPIO_LCD_SCL != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: LCD_SCL / NUT_CHINH");
_Static_assert(CONFIG_GAS_GPIO_RELAY != CONFIG_GAS_GPIO_NUT_BAT, "Trung GPIO: RELAY / NUT_BAT");
_Static_assert(CONFIG_GAS_GPIO_RELAY != CONFIG_GAS_GPIO_NUT_TAT, "Trung GPIO: RELAY / NUT_TAT");
_Static_assert(
    CONFIG_GAS_GPIO_RELAY != CONFIG_GAS_GPIO_NUT_CHE_DO, "Trung GPIO: RELAY / NUT_CHE_DO");
_Static_assert(CONFIG_GAS_GPIO_RELAY != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: RELAY / NUT_CHINH");
_Static_assert(CONFIG_GAS_GPIO_NUT_BAT != CONFIG_GAS_GPIO_NUT_TAT, "Trung GPIO: NUT_BAT / NUT_TAT");
_Static_assert(
    CONFIG_GAS_GPIO_NUT_BAT != CONFIG_GAS_GPIO_NUT_CHE_DO, "Trung GPIO: NUT_BAT / NUT_CHE_DO");
_Static_assert(
    CONFIG_GAS_GPIO_NUT_BAT != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: NUT_BAT / NUT_CHINH");
_Static_assert(
    CONFIG_GAS_GPIO_NUT_TAT != CONFIG_GAS_GPIO_NUT_CHE_DO, "Trung GPIO: NUT_TAT / NUT_CHE_DO");
_Static_assert(
    CONFIG_GAS_GPIO_NUT_TAT != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: NUT_TAT / NUT_CHINH");
_Static_assert(
    CONFIG_GAS_GPIO_NUT_CHE_DO != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: NUT_CHE_DO / NUT_CHINH");

/* ADC1: GPIO36=CH0, GPIO39=CH3, GPIO32..35=CH4..7. */
#define CHAN_MQ2_ADC                                                                               \
    (CONFIG_GAS_GPIO_MQ2 == 36 ? 0 : CONFIG_GAS_GPIO_MQ2 == 39 ? 3 : CONFIG_GAS_GPIO_MQ2 - 28)
