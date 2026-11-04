
#include "GxEPDDisplay.h"

#if defined(LUFREE_EINK)
  #include <new>
  #include "EPaperScan.h"
#endif

#ifdef EXP_PIN_BACKLIGHT
  #include <PCA9557.h>
  extern PCA9557 expander;
#endif

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 3
#endif

#ifdef ESP32
  SPIClass SPI1 = SPIClass(FSPI);
#endif

#if defined(LUFREE_EINK)
// LUFREE: хранилище объекта GxEPD2 (конструируется placement-new в begin()
// после автоподбора пинов экрана).
alignas(8) static uint8_t s_epd_storage[sizeof(GxEPDDisplay::EPD_T) + 8];
void* GxEPDDisplay::epdStorage() { return s_epd_storage; }
#endif

bool GxEPDDisplay::begin() {
#if defined(LUFREE_EINK)
  // LUFREE: распиновка экрана неизвестна — прошивка подбирает её сама.
  // 1) быстрая проверка сохранённой конфигурации (InternalFS /epaper.cfg);
  // 2) полный автоскан (EPaperScan): битбэнг SWRESET/PowerOn + наблюдение
  //    BUSY, LED быстро мигает, до 10 минут; результат сохраняется.
  // Панель не найдена => _absent=true, все операции no-op, BLE/LoRa работают.
  EPaperPins p = lufreeLoadPins();
  if (!p.valid() || !ePaperPingPins(p)) {
    p = ePaperAutoDetect();
    if (p.valid()) lufreeSavePins(p);
  }
  if (!p.valid()) {
    _absent = true;
    _init = false;
    return false;
  }
  // MISO экрану не нужен (модуль без MISO) — подставляем CLK как заглушку.
  SPI1.setPins(p.clk, p.clk, p.din);
  new (epdStorage()) EPD_T(EINK_DISPLAY_MODEL(p.cs, p.dc, p.rst, p.busy));
  _absent = false;
#endif
  display.epd2.selectSPI(SPI1, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#ifdef ESP32
  SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS);
#else
  SPI1.begin();
#endif
  display.init(115200, true, 2, false);
  display.setRotation(DISPLAY_ROTATION);
  setTextSize(1);  // Default to size 1
  display.setPartialWindow(0, 0, display.width(), display.height());

  display.fillScreen(GxEPD_WHITE);
  display.display(true);
  #if DISP_BACKLIGHT
  digitalWrite(DISP_BACKLIGHT, LOW);
  pinMode(DISP_BACKLIGHT, OUTPUT);
  #endif
  _init = true;
  return true;
}

void GxEPDDisplay::turnOn() {
#if defined(LUFREE_EINK)
  if (_absent) { _isOn = true; return; }
#endif
  if (!_init) begin();
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, HIGH);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, HIGH);
#endif
  _isOn = true;
}

void GxEPDDisplay::turnOff() {
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, LOW);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, LOW);
#endif
  _isOn = false;
}

void GxEPDDisplay::clear() {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
}

void GxEPDDisplay::startFrame(Color bkg) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(_curr_color = GxEPD_BLACK);
  display_crc.reset();
}

void GxEPDDisplay::setTextSize(int sz) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display_crc.update<int>(sz);
  switch(sz) {
    case 1:  // Small
      display.setFont(&FreeSans9pt7b);
      break;
    case 2:  // Medium Bold
      display.setFont(&FreeSansBold12pt7b);
      break;
    case 3:  // Large
      display.setFont(&FreeSans18pt7b);
      break;
    default:
      display.setFont(&FreeSans9pt7b);
      break;
  }
}

void GxEPDDisplay::setColor(Color c) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display_crc.update<Color> (c);
  // colours need to be inverted for epaper displays
  if (c == DARK) {
    display.setTextColor(_curr_color = GxEPD_WHITE);
  } else {
    display.setTextColor(_curr_color = GxEPD_BLACK);
  }
}

void GxEPDDisplay::setCursor(int x, int y) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display.setCursor((x+offset_x)*scale_x, (y+offset_y)*scale_y);
}

void GxEPDDisplay::print(const char* str) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display_crc.update<char>(str, strlen(str));
  display.print(str);
}

void GxEPDDisplay::fillRect(int x, int y, int w, int h) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.fillRect(x*scale_x, y*scale_y, w*scale_x, h*scale_y, _curr_color);
}

void GxEPDDisplay::drawRect(int x, int y, int w, int h) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.drawRect(x*scale_x, y*scale_y, w*scale_x, h*scale_y, _curr_color);
}

void GxEPDDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
#if defined(LUFREE_EINK)
  if (_absent) return;
#endif
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display_crc.update<uint8_t>(bits, w * h / 8);
  // Calculate the base position in display coordinates
  uint16_t startX = x * scale_x;
  uint16_t startY = y * scale_y;
  
  // Width in bytes for bitmap processing
  uint16_t widthInBytes = (w + 7) / 8;
  
  // Process the bitmap row by row
  for (uint16_t by = 0; by < h; by++) {
    // Calculate the target y-coordinates for this logical row
    int y1 = startY + (int)(by * scale_y);
    int y2 = startY + (int)((by + 1) * scale_y);
    int block_h = y2 - y1;
    
    // Scan across the row bit by bit
    for (uint16_t bx = 0; bx < w; bx++) {
      // Calculate the target x-coordinates for this logical column
      int x1 = startX + (int)(bx * scale_x);
      int x2 = startX + (int)((bx + 1) * scale_x);
      int block_w = x2 - x1;
      
      // Get the current bit
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      bool bitSet = pgm_read_byte(bits + byteOffset) & bitMask;
      
      // If the bit is set, draw a block of pixels
      if (bitSet) {
        // Draw the block as a filled rectangle
        display.fillRect(x1, y1, block_w, block_h, _curr_color);
      }
    }
  }
}

uint16_t GxEPDDisplay::getTextWidth(const char* str) {
#if defined(LUFREE_EINK)
  if (_absent) return 0;
#endif
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return ceil((w + 1) / scale_x);
}

void GxEPDDisplay::endFrame() {
  uint32_t crc = display_crc.finalize();
#if defined(LUFREE_EINK)
  if (_absent) { last_display_crc_value = crc; return; }
#endif
  if (crc != last_display_crc_value) {
    display.display(true);
    last_display_crc_value = crc;
  }
}
