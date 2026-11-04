/*
 * variant.h — LuFree (luchik): Heltec HT-RA62 clone (nRF52840 + SX1262)
 *             + WeAct Studio 2.9" e-paper 128x296 + 5-way stick + notify LED.
 *
 * ВСЕ конкретные номера пинов — заготовки: сверить с платой!
 * (пользователь не подтвердил раскладку; см. docs/lufree/README.md)
 */

#pragma once

#include "WVariant.h"

////////////////////////////////////////////////////////////////////////////////
// Low frequency clock source

#define USE_LFRC    // 32.768 kHz RC oscillator (как в официальном variants/promicro:
                    // на платах этого семейства кристалл 32кГц не используется; LFXO вешал SD при старте)
#define VARIANT_MCK (64000000ul)

////////////////////////////////////////////////////////////////////////////////
// Power / battery  (PLACEHOLDER - сверить с платой)

#define BATTERY_PIN             (0 + 31)
#define PIN_VBAT_READ           BATTERY_PIN
#define ADC_MULTIPLIER          (4.90F)
#define ADC_RESOLUTION          (14)
#define BATTERY_SENSE_RES       (12)
#define AREF_VOLTAGE            (3.0)

////////////////////////////////////////////////////////////////////////////////
// Number of pins

#define PINS_COUNT              (48)
#define NUM_DIGITAL_PINS        (48)
#define NUM_ANALOG_INPUTS       (1)
#define NUM_ANALOG_OUTPUTS      (0)

////////////////////////////////////////////////////////////////////////////////
// UART pin definition  (PLACEHOLDER - сверить с платой)

#define PIN_SERIAL1_RX          (37)
#define PIN_SERIAL1_TX          (39)

////////////////////////////////////////////////////////////////////////////////
// I2C pin definition  (PLACEHOLDER - сверить с платой)

#define WIRE_INTERFACES_COUNT   (1)
#define PIN_WIRE_SDA            (26)
#define PIN_WIRE_SCL            (27)

////////////////////////////////////////////////////////////////////////////////
// Notification LED (не подсветка!)  (PLACEHOLDER - сверить с платой)

// LED платы семейства Nologo ProMicro — физически P0.15 (карта official variants/promicro,
// где Arduino-пин 22 = P0.15). В нашей identity-карте N = P0/P1.N, поэтому пишем (0 + 15).
#define PIN_LED_NOTIFY          (0 + 15)
#define LED_BUILTIN             PIN_LED_NOTIFY
#define PIN_STATUS_LED          PIN_LED_NOTIFY
#define PIN_LED                 LED_BUILTIN
#define LED_STATE_ON            1   // сверить: активный уровень диода

#define LED_RED                 (-1)   // нет отдельных RGB-LED
#define LED_GREEN               (-1)
#define LED_BLUE                (-1)   // prevents Bluefruit flashing an LED during advertising

////////////////////////////////////////////////////////////////////////////////
// 5-позиционный стик (5 GPIO к GND, INPUT_PULLUP внутри MomentaryButton)
// (PLACEHOLDER - сверить с платой)

#define PIN_STICK_UP            (17)
#define PIN_STICK_DOWN          (18)
#define PIN_STICK_LEFT          (19)
#define PIN_STICK_RIGHT         (20)
#define PIN_STICK_OK            (21)

#define PIN_USER_BTN            PIN_STICK_OK   // OK стика = основная кнопка
#define BUTTON_PIN              PIN_USER_BTN

////////////////////////////////////////////////////////////////////////////////
// SPI0 — радио SX1262  (PLACEHOLDER - сверить с платой)

#define SPI_INTERFACES_COUNT    (2)

#define USE_SX1262
#define SX126X_CS               (0 + 7)
#define SX126X_DIO1             (0 + 8)
#define SX126X_RESET            (0 + 9)
#define SX126X_BUSY             (0 + 10)
#define SX126X_DIO2_AS_RF_SWITCH true
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

#define PIN_SPI_MISO            (0 + 6)
#define PIN_SPI_MOSI            (0 + 5)
#define PIN_SPI_SCK             (0 + 4)
#define PIN_SPI_NSS             SX126X_CS

////////////////////////////////////////////////////////////////////////////////
// SPI1 — e-paper WeAct 2.9" 128x296  (PLACEHOLDER - сверить с платой)

// Пины панели держим здесь (variant.h): GxEPDDisplay.cpp включает их через
// Arduino.h -> variant.h, из target.h они этому файлу не видны.
#define PIN_DISPLAY_CS          (13)   // PLACEHOLDER - сверить с платой
#define PIN_DISPLAY_DC          (14)   // PLACEHOLDER - сверить с платой
#define PIN_DISPLAY_RST         (15)   // PLACEHOLDER - сверить с платой
#define PIN_DISPLAY_BUSY        (16)   // PLACEHOLDER - сверить с платой
#define PIN_DISPLAY_SCK         PIN_SPI1_SCK
#define PIN_DISPLAY_MOSI        PIN_SPI1_MOSI

#define PIN_SPI1_MISO           (-1)   // e-paper MISO не нужен
#define PIN_SPI1_MOSI           (0 + 12)
#define PIN_SPI1_SCK            (0 + 11)

// GxEPD2 needs that for a panel that is not even used !
extern const int MISO;
extern const int MOSI;
extern const int SCK;

////////////////////////////////////////////////////////////////////////////////
// Misc

#undef HAS_GPS
#define HAS_GPS 0
#define HAS_RTC 0
