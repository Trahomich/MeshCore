#include <Arduino.h>
#include <Wire.h>

#include "target.h"
#include <helpers/ArduinoHelpers.h>

LuFreeBoard board;

void LuFreeBoard::begin() {
  NRF52Board::begin();
  Serial.begin(115200);
  pinMode(PIN_VBAT_READ, INPUT);

  pinMode(PIN_USER_BTN, INPUT);

  // notification LED (не подсветка)
  pinMode(PIN_LED_NOTIFY, OUTPUT);
  digitalWrite(PIN_LED_NOTIFY, !LED_STATE_ON);   // off at boot
}

RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);

WRAPPER_CLASS radio_driver(radio, board);

SensorManager sensors = SensorManager();

VolatileRTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  // стик OK: активен LOW, внутренняя подтяжка INPUT_PULLUP
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true, true, false);
  #ifdef UI_JOYSTICK_5WAY
    MomentaryButton joystick_up(PIN_STICK_UP, 1000, true, true, false);
    MomentaryButton joystick_down(PIN_STICK_DOWN, 1000, true, true, false);
    MomentaryButton joystick_left(PIN_STICK_LEFT, 1000, true, true, false);
    MomentaryButton joystick_right(PIN_STICK_RIGHT, 1000, true, true, false);
    MomentaryButton back_btn(PIN_STICK_LEFT, 1000, true, true, true);
  #endif
#endif

bool radio_init() {
  return radio.std_init(&SPI);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}
