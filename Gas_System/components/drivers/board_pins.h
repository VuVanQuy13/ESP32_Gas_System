#pragma once
#include "sdkconfig.h"
/* Chính sách chân cho ESP32 classic/WROOM: tránh flash, UART0 và strapping.
 * Nút cần pull-up; LCD/relay cần output. GPIO34..39 chỉ dành ADC input. */
#define IS_VALID_OUTPUT_GPIO(gpio)                                                                       \
    ((gpio) == 4 || (gpio) == 13 || (gpio) == 14 || ((gpio) >= 16 && (gpio) <= 19) || ((gpio) >= 21 && (gpio) <= 23) || \
        ((gpio) >= 25 && (gpio) <= 27) || (gpio) == 32 || (gpio) == 33)
_Static_assert(
    (CONFIG_GAS_GPIO_MQ2 >= 32 && CONFIG_GAS_GPIO_MQ2 <= 36) || CONFIG_GAS_GPIO_MQ2 == 39,
    "GPIO MQ2 khong hop le; xem menu Chan GPIO");
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_LCD_SDA), "GPIO LCD_SDA khong hop le; xem menu Chan GPIO");
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_LCD_SCL), "GPIO LCD_SCL khong hop le; xem menu Chan GPIO");
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_RELAY), "GPIO RELAY khong hop le; xem menu Chan GPIO");
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_NUT_BAT), "GPIO NUT_BAT khong hop le; xem menu Chan GPIO");
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_NUT_TAT), "GPIO NUT_TAT khong hop le; xem menu Chan GPIO");
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_NUT_CHE_DO), "GPIO NUT_CHE_DO khong hop le; xem menu Chan GPIO");
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_NUT_CHINH), "GPIO NUT_CHINH khong hop le; xem menu Chan GPIO");
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

/* Cam bien lua chi can input; GPIO34..39 la input-only, khong co pull noi. */
_Static_assert(
    IS_VALID_OUTPUT_GPIO(CONFIG_GAS_GPIO_FIRE_D0) ||
        (CONFIG_GAS_GPIO_FIRE_D0 >= 34 && CONFIG_GAS_GPIO_FIRE_D0 <= 39),
    "GPIO FIRE_D0 khong hop le; xem menu Chan GPIO");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_MQ2, "Trung GPIO: FIRE_D0 / MQ2");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_LCD_SDA, "Trung GPIO: FIRE_D0 / LCD_SDA");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_LCD_SCL, "Trung GPIO: FIRE_D0 / LCD_SCL");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_RELAY, "Trung GPIO: FIRE_D0 / RELAY");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_NUT_BAT, "Trung GPIO: FIRE_D0 / NUT_BAT");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_NUT_TAT, "Trung GPIO: FIRE_D0 / NUT_TAT");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_NUT_CHE_DO, "Trung GPIO: FIRE_D0 / NUT_CHE_DO");
_Static_assert(CONFIG_GAS_GPIO_FIRE_D0 != CONFIG_GAS_GPIO_NUT_CHINH, "Trung GPIO: FIRE_D0 / NUT_CHINH");

/* ADC1: GPIO36=CH0, GPIO39=CH3, GPIO32..35=CH4..7. */
#define MQ2_ADC_CHANNEL                                                                            \
    (CONFIG_GAS_GPIO_MQ2 == 36 ? 0 : CONFIG_GAS_GPIO_MQ2 == 39 ? 3 : CONFIG_GAS_GPIO_MQ2 - 28)
