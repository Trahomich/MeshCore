#pragma once

#include <Arduino.h>
#include <MeshCore.h>
#include <helpers/NRF52Board.h>

// LuFree ("luchik"): Heltec HT-RA62 clone (nRF52840 + SX1262)

#define  MV_LSB   (3000.0F / 4096.0F) // 12-bit ADC with 3.0V input range

class LuFreeBoard : public NRF52BoardDCDC {
public:
  LuFreeBoard() : NRF52Board("LUFREE_OTA") {}
  void begin();

  uint16_t getBattMilliVolts() override {
    // PLACEHOLDER: схема замера батареи неизвестна — сверить с платой
    int adcvalue = 0;
    analogReadResolution(12);
    analogReference(AR_INTERNAL_3_0);
    delay(10);
    adcvalue = analogRead(PIN_VBAT_READ);
    return (uint16_t)((float)adcvalue * MV_LSB * ADC_MULTIPLIER);
  }

  const char* getManufacturerName() const override {
    return "LuFree";
  }

  void powerOff() override {
    sd_power_system_off();
  }
};
