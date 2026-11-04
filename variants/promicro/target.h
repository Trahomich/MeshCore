#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <PromicroBoard.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#ifdef DISPLAY_CLASS
  #if defined(LUFREE_EINK)
    #include <helpers/ui/GxEPDDisplay.h>   // LuFree: e-paper WeAct 2.9" (SSD1680)
  #else
    #include <helpers/ui/SSD1306Display.h>
  #endif
  #include <helpers/ui/MomentaryButton.h>
#endif

#include <helpers/sensors/EnvironmentSensorManager.h>

extern PromicroBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  #if defined(LUFREE_STICK)
    // LuFree: пины стика определяются в рантайме (variants/lufree/StickScan),
    // поэтому кнопки — ссылки на placement-new объекты, конструируемые в
    // lufreeStickSetup() ДО ui_task.begin(). Определения — StickScan.cpp.
    extern MomentaryButton& user_btn;        // стик OK -> KEY_ENTER
    #if defined(UI_JOYSTICK_5WAY)
      extern MomentaryButton& joystick_up;
      extern MomentaryButton& joystick_down;
      extern MomentaryButton& joystick_left;
      extern MomentaryButton& joystick_right;
      extern MomentaryButton& back_btn;      // тот же пин, что joystick_left (тройной клик = меню)
    #endif
  #else
    extern MomentaryButton user_btn;
  #endif
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();

