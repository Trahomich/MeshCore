// LuFree: автоподбор пинов e-paper WeAct 2.9" (SSD1680, GxEPD2_290_GDEY029T94).
//
// Метод (см. обсуждение задачи #14): реальная разводка неизвестна, панель ищем:
//   1) BUSY-кандидаты: пины, чьё чтение НЕ меняется при смене подтяжки
//      (INPUT_PULLUP/INPUT_PULLDOWN) — т.е. линия чем-то управляется.
//      Висящий пин следует за подтяжкой и отбрасывается.
//   2) Оракул: посылка команд SWRESET (0x12) и Power On (0x04) битбэнгом на
//      гипотезе (CLK,DIN,CS,DC); реальная панель отвечает импульсом BUSY
//      (уровень busy_level=HIGH, idle=LOW) в окне ~100 мс. Попадание
//      подтверждается повтором.
//   3) Порядок гипотез: (а) «окна» из 4 подряд идущих свободных пинов в порядке
//      шлейфа модуля CLK DIN CS DC (прямой и обратный), (б) типичные пары шины
//      × любые CS/DC, (в) полный перебор до таймаута.
// LED при поиске быстро мигает (PIN_LED из варианта).

#include <Arduino.h>
#include "EPaperScan.h"
#include <Adafruit_LittleFS.h>

using namespace Adafruit_LittleFS_Namespace;

extern Adafruit_LittleFS InternalFS;   // ядро Adafruit nRF52

namespace {

// все пины, допустимые для экрана (Arduino-номера варианта promicro).
// Занятые радио/LED/батареей/кнопкой НЕ включены: 10,11,13,16 (LoRa ctrl),
// 17 (батарея), 21 (POWER_EN), 22 (LED), 6 (кнопка fakeTec).
// 2,3,4 — аппаратная SPI0 (шина радио): до radio_init ещё никем не занята,
// допускаются (экран мог быть посажен на общую шину).
// 12,14,15 — по документации mrekin это «шина радио», фактически свободны.
// 0,1 — Serial1 (GPS отключён), 18,19,20 — SPI1.
const int8_t ALL_PINS[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 12, 14, 15, 18, 19, 20};
const int N_ALL = sizeof(ALL_PINS) / sizeof(ALL_PINS[0]);

// типичные пары (CLK, DIN) — подписи SPI на платах этого семейства
const int8_t BUS_PAIRS[][2] = {
  {2, 4}, {4, 2},          // SPI0 (SCK=2, MOSI=4)
  {12, 14}, {14, 12},      // mrekin: SCLK=P1.11(12), MOSI=P1.15(14)
  {18, 20}, {20, 18},      // SPI1
  {0, 1}, {1, 0}, {7, 8}, {8, 7}, {8, 9}, {9, 8}
};
const int N_PAIRS = sizeof(BUS_PAIRS) / sizeof(BUS_PAIRS[0]);

const uint32_t SCAN_CAP_MS = 10UL * 60UL * 1000UL;   // общий лимит сканирования

struct BusyCand {
  int8_t pin;
  uint8_t idle;      // уровень покоя (управляется панелью)
};

bool usedAsBusy[32];

void ledTick() {
#ifdef PIN_LED
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 100) {
    last = now;
    digitalWrite(PIN_LED, (now / 100) & 1 ? LOW : HIGH);
  }
#endif
}

void sendByte(uint8_t v, int8_t clk, int8_t din) {
  for (int8_t i = 7; i >= 0; i--) {
    digitalWrite(clk, LOW);
    digitalWrite(din, (v >> i) & 1);
    delayMicroseconds(5);
    digitalWrite(clk, HIGH);   // SSD1680 защёлкивает по фронту
    delayMicroseconds(5);
  }
}

void sendCmd(uint8_t cmd, int8_t clk, int8_t din, int8_t cs, int8_t dc) {
  digitalWrite(dc, LOW);       // команда
  digitalWrite(cs, LOW);
  sendByte(cmd, clk, din);
  digitalWrite(cs, HIGH);
}

// следим за всеми BUSY-кандидатами окно window_ms; вернём true и пин,
// который дольше прочего отклонился от своего idle-уровня
bool watchBusy(BusyCand *c, int nc, uint32_t window_ms, int8_t &hitPin) {
  uint8_t seen[16] = {0};
  uint32_t t0 = millis();
  while (millis() - t0 < window_ms) {
    for (int i = 0; i < nc; i++) {
      if (digitalRead(c[i].pin) != c[i].idle) {
        if (seen[i] < 255) seen[i]++;
      }
    }
    delayMicroseconds(500);
  }
  int best = -1, bestv = 0;
  for (int i = 0; i < nc; i++) {
    if (seen[i] > bestv) { bestv = seen[i]; best = i; }
  }
  if (best >= 0 && bestv >= 4) {   // >= 2 мс отклонения
    hitPin = c[best].pin;
    return true;
  }
  return false;
}

// одна попытка: команды на гипотезу пинов + наблюдение BUSY (2 повтора)
bool attempt(const int8_t *pins /* clk,din,cs,dc */, BusyCand *c, int nc, int8_t &busyHit) {
  int8_t clk = pins[0], din = pins[1], cs = pins[2], dc = pins[3];
  if (usedAsBusy[clk] || usedAsBusy[din] || usedAsBusy[cs] || usedAsBusy[dc]) return false;
  if (clk == din || clk == cs || clk == dc || din == cs || din == dc || cs == dc) return false;

  pinMode(clk, OUTPUT); digitalWrite(clk, LOW);
  pinMode(din, OUTPUT); digitalWrite(din, LOW);
  pinMode(cs, OUTPUT);  digitalWrite(cs, HIGH);
  pinMode(dc, OUTPUT);

  // ждём окончания возможной предыдущей активности панели
  for (int i = 0; i < nc; i++) { while (digitalRead(c[i].pin) != c[i].idle) delay(1); }

  int8_t hit1 = -1;
  sendCmd(0x12, clk, din, cs, dc);          // SWRESET
  delay(10);
  sendCmd(0x04, clk, din, cs, dc);          // Power On -> BUSY-импульс
  if (!watchBusy(c, nc, 80, hit1)) return false;

  // подтверждение повтором
  delay(5);
  int8_t hit2 = -1;
  sendCmd(0x12, clk, din, cs, dc);
  delay(10);
  sendCmd(0x04, clk, din, cs, dc);
  if (!watchBusy(c, nc, 80, hit2)) return false;
  if (hit1 != hit2) return false;

  busyHit = hit1;
  return true;
}

} // namespace

EPaperPins ePaperAutoDetect() {
  EPaperPins out = {-1, -1, -1, -1, -1, -1};

#ifdef PIN_LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
#endif

  // 1) BUSY-кандидаты: управляемые линии
  BusyCand cand[16];
  int nc = 0;
  for (int i = 0; i < N_ALL && nc < 16; i++) {
    int8_t p = ALL_PINS[i];
    pinMode(p, INPUT_PULLUP);   delayMicroseconds(60);  uint8_t v1 = digitalRead(p);
    pinMode(p, INPUT_PULLDOWN); delayMicroseconds(60);  uint8_t v2 = digitalRead(p);
    pinMode(p, INPUT);          // вернуть в нейтраль
    if (v1 == v2) { cand[nc].pin = p; cand[nc].idle = v1; nc++; }
  }
  if (nc == 0) return out;

  for (int i = 0; i < 32; i++) usedAsBusy[i] = false;
  // пин, назначенный BUSY, нельзя использовать как управляющий
  for (int i = 0; i < nc; i++) usedAsBusy[cand[i].pin] = true;

  int8_t pins[4], busyHit = -1;
  uint32_t t0 = millis();

  // 2а) окна из 4 подряд идущих пинов (прямой и обратный порядок шлейфа)
  for (int dir = 0; dir < 2 && busyHit < 0; dir++) {
    for (int i = 0; i + 3 < N_ALL; i++) {
      ledTick();
      if (millis() - t0 > SCAN_CAP_MS) return out;
      if (dir == 0) { pins[0]=ALL_PINS[i]; pins[1]=ALL_PINS[i+1]; pins[2]=ALL_PINS[i+2]; pins[3]=ALL_PINS[i+3]; }
      else          { pins[0]=ALL_PINS[i+3]; pins[1]=ALL_PINS[i+2]; pins[2]=ALL_PINS[i+1]; pins[3]=ALL_PINS[i]; }
      if (attempt(pins, cand, nc, busyHit)) break;
    }
  }

  // 2б) типичные пары шины × любые CS/DC
  if (busyHit < 0) {
    for (int b = 0; b < N_PAIRS && busyHit < 0; b++) {
      for (int csi = 0; csi < N_ALL && busyHit < 0; csi++) {
        for (int dci = 0; dci < N_ALL; dci++) {
          ledTick();
          if (millis() - t0 > SCAN_CAP_MS) return out;
          pins[0] = BUS_PAIRS[b][0]; pins[1] = BUS_PAIRS[b][1];
          pins[2] = ALL_PINS[csi];   pins[3] = ALL_PINS[dci];
          if (attempt(pins, cand, nc, busyHit)) break;
        }
      }
    }
  }

  // 2в) полный перебор до лимита времени
  if (busyHit < 0) {
    for (int i = 0; i < N_ALL && busyHit < 0; i++) {
      for (int j = 0; j < N_ALL && busyHit < 0; j++) {
        if (j == i) continue;
        for (int k = 0; k < N_ALL && busyHit < 0; k++) {
          if (k == i || k == j) continue;
          for (int l = 0; l < N_ALL; l++) {
            ledTick();
            if (millis() - t0 > SCAN_CAP_MS) return out;
            pins[0]=ALL_PINS[i]; pins[1]=ALL_PINS[j]; pins[2]=ALL_PINS[k]; pins[3]=ALL_PINS[l];
            if (attempt(pins, cand, nc, busyHit)) break;
          }
        }
      }
    }
  }

  if (busyHit >= 0) {
    out.clk = pins[0]; out.din = pins[1]; out.cs = pins[2]; out.dc = pins[3];
    out.rst = -1;          // RST не ищем: SWRESET достаточно (deep sleep не используем)
    out.busy = busyHit;
  }
  return out;
}

// быстрая проверка сохранённых пинов (та же пара команд)
bool ePaperPingPins(const EPaperPins &p) {
  if (!p.valid() || p.busy < 0) return false;
  BusyCand c = { p.busy, 0 };
  pinMode(p.busy, INPUT);   delayMicroseconds(100);
  c.idle = digitalRead(p.busy);
  int8_t hit = -1;
  BusyCand arr[1] = { c };
  int8_t pins[4] = { p.clk, p.din, p.cs, p.dc };
  return attempt(pins, arr, 1, hit) && hit == p.busy;
}

EPaperPins lufreeLoadPins() {
  EPaperPins out = {-1, -1, -1, -1, -1, -1};
  InternalFS.begin();
  File f(InternalFS.open("/epaper.cfg", FILE_O_READ));
  if (f) {
    uint8_t buf[10];
    size_t n = f.read(buf, sizeof(buf));
    f.close();
    if (n == 10 && buf[0] == 0x4C && buf[1] == 0x45) {
      out.clk = buf[2]; out.din = buf[3]; out.cs = buf[4];
      out.dc = buf[5]; out.rst = buf[6]; out.busy = buf[7];
    }
  }
  return out;
}

void lufreeSavePins(const EPaperPins &p) {
  InternalFS.begin();
  File f(InternalFS.open("/epaper.cfg", FILE_O_WRITE));
  if (f) {
    uint8_t buf[10] = {0x4C, 0x45, (uint8_t)p.clk, (uint8_t)p.din, (uint8_t)p.cs,
                       (uint8_t)p.dc, (uint8_t)p.rst, (uint8_t)p.busy, 0, 0};
    f.write(buf, sizeof(buf));
    f.close();
  }
}
