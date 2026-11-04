#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/LocationProvider.h>
#include "LuFreeBoard.h"

// Пины радио SX1262 (PLACEHOLDER - сверить с платой; если на модуле LR1110 -
// переключить на CustomLR1110 по образцу variants/wio_wm1110)
#define P_LORA_DIO_1   SX126X_DIO1
#define P_LORA_NSS     SX126X_CS
#define P_LORA_RESET   SX126X_RESET
#define P_LORA_BUSY    SX126X_BUSY
#define P_LORA_SCLK    PIN_SPI_SCK
#define P_LORA_MISO    PIN_SPI_MISO
#define P_LORA_MOSI    PIN_SPI_MOSI

// Пины e-paper WeAct 2.9" 128x296 — в variant.h (см. ниже): GxEPDDisplay.cpp
// получает их оттуда через Arduino.h → variant.h; из target.h они этому
// translation unit не видны (так же устроены mesh_pocket, wio-tracker-l1).

// --- e-paper WeAct 2.9" 128x296: выбор контроллера одним define ------------
// Контроллер модуля неизвестен (на плате только "WeAct Studio"). Дефолт SSD1680.
// EINK_DISPLAY_MODEL и EINK_SCALE_*/OFFSET задаются флагами -D в platformio.ini
// (секция [lufree]): GxEPDDisplay.cpp компилируется вне target.h, локальный
// #define здесь на него не подействует — так же устроены mesh_pocket/wio-tracker-l1-eink.
// Если экран не оживает или виснет на BUSY — переключить модель в platformio.ini.
// Доступные классы и обоснование параметров: docs/lufree/research-epaper.md.

// Поворот панели задаётся флагом -D DISPLAY_ROTATION= в platformio.ini
// (GxEPDDisplay.cpp читает его напрямую, из target.h он не виден).


#ifdef DISPLAY_CLASS
  #if defined(LUFREE_USE_GXEPD)
    #include <helpers/ui/GxEPDDisplay.h>
  #else
    #include <helpers/ui/NullDisplayDriver.h>
  #endif
  #include <helpers/ui/MomentaryButton.h>
#endif

extern LuFreeBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  extern MomentaryButton user_btn;
  #ifdef UI_JOYSTICK_5WAY
    // LuFree: 5-позиционный стик (up/down/left/right/ok), пины - variants/lufree/variant.h
    extern MomentaryButton joystick_up;
    extern MomentaryButton joystick_down;
    extern MomentaryButton joystick_left;
    extern MomentaryButton joystick_right;
    extern MomentaryButton back_btn;   // отдельного нет: на пине LEFT (тройной клик = контекстное меню)
  #endif
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();

extern SensorManager sensors;
