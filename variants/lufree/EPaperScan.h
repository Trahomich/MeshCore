#pragma once

// LuFree ("luchik"): автоподбор пинов e-paper WeAct 2.9" (SSD1680).
// Реальная разводка устройства неизвестна (экран подключён навесным монтажом
// к плате ProMicro/fakeTec), поэтому прошивка ищет панель сама при первом
// старте и сохраняет результат в InternalFS (файл /epaper.cfg).
//
// Все номера — Arduino-пины варианта promicro (см. variants/promicro/variant.cpp).

struct EPaperPins {
  int8_t clk, din, cs, dc, rst, busy;
  bool valid() const { return clk >= 0 && cs >= 0; }
};

// Сканирует свободные пины и возвращает найденную панель.
// Если панель не найдена — valid()==false. Длительность: от долей секунды до
// нескольких минут (быстрое мигание LED во время поиска).
EPaperPins ePaperAutoDetect();

// Свободный пин-заглушка для MISO (nrfx_spim конфигурирует MISO входом
// ПОСЛЕ SCK-выхода, поэтому подставлять найденный пин нельзя).
int8_t ePaperStubMiso(const EPaperPins &p);

// Быстрая проверка сохранённых пинов (SWRESET/PowerOn + BUSY-отклик).
bool ePaperPingPins(const EPaperPins &p);

// Загрузка/сохранение найденных пинов (InternalFS, файл /epaper.cfg).
EPaperPins lufreeLoadPins();
void lufreeSavePins(const EPaperPins &p);

// Список свободных пинов (ALL_PINS из EPaperScan.cpp) — общий для сканеров
// периферии (экран, стик): LuFree один источник правды о свободных GPIO.
const int8_t* lufreeFreePins(int &count);
