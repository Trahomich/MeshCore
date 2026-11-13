#include <Arduino.h>
#include "target.h"
#include <helpers/ArduinoHelpers.h>

PromicroBoard board;

RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);

WRAPPER_CLASS radio_driver(radio, board);

VolatileRTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
#if ENV_INCLUDE_GPS
  #include <helpers/sensors/MicroNMEALocationProvider.h>
  MicroNMEALocationProvider nmea = MicroNMEALocationProvider(Serial1, &rtc_clock);
  EnvironmentSensorManager sensors = EnvironmentSensorManager(nmea);
#else
  EnvironmentSensorManager sensors;
#endif

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  #if defined(LUFREE_STICK)
    // LuFree: user_btn/joystick_*/back_btn — ссылки, объявленные в target.h;
    // определяются в variants/lufree/StickScan.cpp (рантайм-пины стика,
    // см. lufreeStickSetup). Здесь объекта не определяем.
  #else
    MomentaryButton user_btn(PIN_USER_BTN, 1000, true, true);
  #endif
#endif

bool radio_init() {
#if !defined(LUFREE_EINK)
  // LuFree: I2C-скан RTC не запускаем — пины Wire могут принадлежать экрану,
  // который ищется автоподбором (GxEPDDisplay::begin -> EPaperScan).
  // Без begin() часы прозрачно падают в VolatileRTCClock (fallback).
  rtc_clock.begin(Wire);
#endif
  return radio.std_init(&SPI);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}


