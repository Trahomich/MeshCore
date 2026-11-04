// LuFree: автоподбор пинов e-paper WeAct 2.9" (SSD1680, GxEPD2_290_GDEY029T94).
//
// Метод (см. обсуждение задачи #14): реальная разводка неизвестна, панель ищем:
//   1) BUSY-кандидаты: пины, чьё чтение НЕ меняется при смене подтяжки
//      (INPUT_PULLUP/INPUT_PULLDOWN) — т.е. линия чем-то управляется.
//      Висящий пин следует за подтяжкой и отбрасывается.
//   2) Оракул: посылка команд SWRESET (0x12) и Power On (0x04) битбэнгом на
//      гипотезе (CLK,DIN,CS,DC); реальная панель отвечает импульсом BUSY
//      (уровень busy_level=HIGH, idle=LOW) в окне ~100 мс с ВОЗВРАТОМ к idle.
//      Попадание подтверждается повтором.
//   3) Порядок гипотез: (а) «окна» из 4 подряд идущих свободных пинов в порядке
//      шлейфа модуля CLK DIN CS DC (прямой и обратный), (а2) «окна» из 6
//      подряд идущих пинов — полный шлейф CLK DIN CS DC RST BUSY (прямой и
//      обратный): RST-пульс будит панель из deep sleep (там SWRESET 0x12
//      игнорируется и стадия (а) её не видит), (б) типичные пары шины
//      × любые CS/DC, (в) полный перебор (BUSY-кандидаты — ВСЕ свободные пины,
//      на случай панели в deep sleep, когда BUSY высокоомный) до таймаута.
// LED при поиске быстро мигает (PIN_LED из варианта).
// Все ожидания ограничены таймаутами: никаких вечных циклов до radio_init.
// На выходе все сканированные пины возвращаются в INPUT_PULLUP (нейтраль).

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

// лимит скана: display.begin() идёт до radio_init, и всё это время нет
// BLE/радио, поэтому полный перебор просто обрезается (первые стадии —
// окна и типичные пары — успевают с запасом)
const uint32_t SCAN_CAP_MS = 90UL * 1000UL;

struct BusyCand {
  int8_t pin;
  uint8_t idle;      // уровень покоя (управляется панелью)
};

// noDrive: пин похож на линию BUSY (управляется снаружи) — нельзя делать
// драйверным (CLK/DIN/CS/DC). stuck: пин застрял не в idle (помеха) —
// исключается из наблюдения совсем.
bool noDrive[32], stuck[32];

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

// все кандидаты вернулись к idle в течение timeout_ms?
bool waitIdle(const BusyCand *c, int nc, uint32_t timeout_ms) {
  uint32_t t0 = millis();
  for (;;) {
    bool all = true;
    for (int i = 0; i < nc; i++) {
      if (digitalRead(c[i].pin) != c[i].idle) { all = false; break; }
    }
    if (all) return true;
    if (millis() - t0 >= timeout_ms) return false;
    delay(1);
  }
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

// одна попытка: команды на гипотезу пинов + наблюдение BUSY (2 повтора).
// rst >= 0 — гипотеза аппаратного RST: пульс LOW 50 мкс -> HIGH, пауза 60 мс.
// Это будит панель из deep sleep (там SWRESET 0x12 игнорируется) и заодно
// аппаратно сбрасывает живую (её BUSY дёрнется уже от резета, waitIdle ниже
// это переварит). rst == -1 — RST не ищем, только SWRESET.
bool attempt(const int8_t *pins /* clk,din,cs,dc */, BusyCand *c, int nc, int8_t &busyHit, int8_t rst = -1) {
  int8_t clk = pins[0], din = pins[1], cs = pins[2], dc = pins[3];
  if (noDrive[clk] || noDrive[din] || noDrive[cs] || noDrive[dc]) return false;
  if (stuck[clk] || stuck[din] || stuck[cs] || stuck[dc]) return false;
  if (clk == din || clk == cs || clk == dc || din == cs || din == dc || cs == dc) return false;
  if (rst >= 0) {
    if (noDrive[rst] || stuck[rst]) return false;
    if (rst == clk || rst == din || rst == cs || rst == dc) return false;
  }

  // рабочая копия кандидатов: без застрявших и без совпадающих с драйверами
  BusyCand c2[16];
  int nc2 = 0;
  for (int i = 0; i < nc && nc2 < 16; i++) {
    if (stuck[c[i].pin]) continue;
    if (c[i].pin == clk || c[i].pin == din || c[i].pin == cs || c[i].pin == dc) continue;
    if (rst >= 0 && c[i].pin == rst) continue;
    c2[nc2++] = c[i];
  }
  if (nc2 == 0) return false;
  c = c2; nc = nc2;

  pinMode(clk, OUTPUT); digitalWrite(clk, LOW);
  pinMode(din, OUTPUT); digitalWrite(din, LOW);
  pinMode(cs, OUTPUT);  digitalWrite(cs, HIGH);
  pinMode(dc, OUTPUT);
  if (rst >= 0) {
    pinMode(rst, OUTPUT);
    digitalWrite(rst, HIGH); delayMicroseconds(10);
    digitalWrite(rst, LOW);  delayMicroseconds(50);
    digitalWrite(rst, HIGH);
    delay(10);   // панель выходит из резета (её BUSY при этом занят несколько мс)
    // записанные idle могли быть прочитаны, пока BUSY панели был высокоомным
    // (deep sleep) — обновляем их по факту: ждём, пока уровень устоится.
    for (int i = 0; i < nc; i++) {
      uint8_t a = digitalRead(c[i].pin);
      uint32_t ts = millis();
      while (millis() - ts < 300) {
        delay(5);
        uint8_t b = digitalRead(c[i].pin);
        if (b == a) break;
        a = b; ts = millis();
      }
      c[i].idle = a;
    }
    delay(50);   // страховка: BUSY после резета обязан вернуться к idle
  }

  // ждём окончания возможной предыдущей активности панели, но НЕ вечно:
  // застрявший не в idle кандидат — помеха (зажатый стик и т.п.), исключаем
  // его совсем, иначе скан зависнет до radio_init (К2).
  {
    int w = 0;
    for (int i = 0; i < nc; i++) {
      BusyCand one[1] = { c[i] };
      if (!waitIdle(one, 1, 200)) { stuck[c[i].pin] = true; continue; }
      c[w++] = c[i];
    }
    nc = w;
    if (nc == 0) return false;
  }

  int8_t hit1 = -1;
  sendCmd(0x12, clk, din, cs, dc);          // SWRESET
  delay(10);
  sendCmd(0x04, clk, din, cs, dc);          // Power On -> BUSY-импульс
  if (!watchBusy(c, nc, 80, hit1)) return false;
  // отклонение обязано закончиться возвратом к idle, иначе это не импульс
  // BUSY, а постоянный уровень (отпущенный стик, наводка) — отвергаем (В2)
  if (!waitIdle(c, nc, 300)) return false;

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

// вернуть все сканированные пины в нейтраль (В4): после неудачного скана
// никто не должен остаться OUTPUT LOW (например, линия подсветки).
// Подтяжка вверх держит CS/CLK панели в неактивном уровне.
static void releaseAllPins() {
  for (int i = 0; i < N_ALL; i++) pinMode(ALL_PINS[i], INPUT_PULLUP);
}

EPaperPins ePaperAutoDetect() {
  EPaperPins out = {-1, -1, -1, -1, -1, -1};

#ifdef PIN_LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
#endif

  for (int i = 0; i < 32; i++) { noDrive[i] = false; stuck[i] = false; }

  // 1) BUSY-кандидаты: управляемые линии
  BusyCand cand[16];
  int nc = 0;
  for (int i = 0; i < N_ALL && nc < 16; i++) {
    int8_t p = ALL_PINS[i];
    pinMode(p, INPUT_PULLUP);   delayMicroseconds(60);  uint8_t v1 = digitalRead(p);
    pinMode(p, INPUT_PULLDOWN); delayMicroseconds(60);  uint8_t v2 = digitalRead(p);
    pinMode(p, INPUT);          // вернуть в нейтраль
    if (v1 == v2) { cand[nc].pin = p; cand[nc].idle = v1; nc++; noDrive[p] = true; }
  }
  // nc == 0 больше не приговор: панель в deep sleep даёт высокоомный BUSY,
  // фильтр подтяжек его отбрасывает — её поднимет RST-пульс в стадии 2а2.

  int8_t pins[4], busyHit = -1, rstFound = -1;
  uint32_t t0 = millis();
  bool timedOut = false;

  // 2а) окна из 4 подряд идущих пинов (прямой и обратный порядок шлейфа)
  for (int dir = 0; nc > 0 && dir < 2 && busyHit < 0 && !timedOut; dir++) {
    for (int i = 0; i + 3 < N_ALL; i++) {
      ledTick();
      if (millis() - t0 > SCAN_CAP_MS) { timedOut = true; break; }
      if (dir == 0) { pins[0]=ALL_PINS[i]; pins[1]=ALL_PINS[i+1]; pins[2]=ALL_PINS[i+2]; pins[3]=ALL_PINS[i+3]; }
      else          { pins[0]=ALL_PINS[i+3]; pins[1]=ALL_PINS[i+2]; pins[2]=ALL_PINS[i+1]; pins[3]=ALL_PINS[i]; }
      if (attempt(pins, cand, nc, busyHit)) break;
    }
  }

  // 2а2) окна из 6 подряд идущих пинов — полный шлейф модуля
  // CLK DIN CS DC RST BUSY (прямой и обратный порядок). RST-пульс будит
  // панель из deep sleep, поэтому BUSY (6-й пин окна) добавляем к кандидатам
  // с живым чтением idle — в исходный cand он не попал (высокоомный).
  if (busyHit < 0 && !timedOut) {
    for (int dir = 0; dir < 2 && busyHit < 0 && !timedOut; dir++) {
      for (int i = 0; i + 5 < N_ALL; i++) {
        ledTick();
        if (millis() - t0 > SCAN_CAP_MS) { timedOut = true; break; }
        if (dir == 0) { pins[0]=ALL_PINS[i];   pins[1]=ALL_PINS[i+1]; pins[2]=ALL_PINS[i+2]; pins[3]=ALL_PINS[i+3]; }
        else          { pins[0]=ALL_PINS[i+5]; pins[1]=ALL_PINS[i+4]; pins[2]=ALL_PINS[i+3]; pins[3]=ALL_PINS[i+2]; }
        int8_t rst = (dir == 0) ? ALL_PINS[i+4] : ALL_PINS[i+1];
        int8_t bp  = (dir == 0) ? ALL_PINS[i+5] : ALL_PINS[i];
        if (stuck[bp] || stuck[rst]) continue;
        BusyCand c6[16];
        int n6 = 0;
        for (int q = 0; q < nc && n6 < 16; q++) {
          if (cand[q].pin == bp || cand[q].pin == rst) continue;
          c6[n6++] = cand[q];
        }
        if (n6 < 16) {
          pinMode(bp, INPUT); delayMicroseconds(100);
          c6[n6].pin = bp; c6[n6].idle = digitalRead(bp); n6++;
        }
        if (n6 == 0) continue;
        int8_t r = rst;
        if (attempt(pins, c6, n6, busyHit, r)) { rstFound = r; break; }
      }
    }
  }

  // панель могла проснуться от RST-пульса в 2а2: её BUSY стал активным, но в
  // исходный cand не попал. Пересобираем список перед 2б (объединяем).
  if (busyHit < 0) {
    for (int i = 0; i < N_ALL && nc < 16; i++) {
      int8_t p = ALL_PINS[i];
      if (stuck[p]) continue;
      bool known = false;
      for (int q = 0; q < nc; q++) if (cand[q].pin == p) { known = true; break; }
      if (known) continue;
      pinMode(p, INPUT_PULLUP);   delayMicroseconds(60);  uint8_t v1 = digitalRead(p);
      pinMode(p, INPUT_PULLDOWN); delayMicroseconds(60);  uint8_t v2 = digitalRead(p);
      pinMode(p, INPUT);
      if (v1 == v2) { cand[nc].pin = p; cand[nc].idle = v1; nc++; noDrive[p] = true; }
    }
  }

  // 2б) типичные пары шины × любые CS/DC
  if (busyHit < 0 && !timedOut) {
    for (int b = 0; b < N_PAIRS && busyHit < 0 && !timedOut; b++) {
      for (int csi = 0; csi < N_ALL && busyHit < 0 && !timedOut; csi++) {
        for (int dci = 0; dci < N_ALL; dci++) {
          ledTick();
          if (millis() - t0 > SCAN_CAP_MS) { timedOut = true; break; }
          pins[0] = BUS_PAIRS[b][0]; pins[1] = BUS_PAIRS[b][1];
          pins[2] = ALL_PINS[csi];   pins[3] = ALL_PINS[dci];
          if (attempt(pins, cand, nc, busyHit)) break;
        }
      }
    }
  }

  // 2в) полный перебор до лимита времени. BUSY-кандидаты здесь — ВСЕ свободные
  // пины (кроме застрявших): если панель осталась в deep sleep, её BUSY
  // высокоомный и фильтр подтяжек его отбросил (В5). Драйверные пины,
  // совпадающие с кандидатами, отбрасываются внутри attempt.
  if (busyHit < 0 && !timedOut) {
    BusyCand any[16];
    int nAny = 0;
    for (int i = 0; i < N_ALL && nAny < 16; i++) {
      if (stuck[ALL_PINS[i]]) continue;
      pinMode(ALL_PINS[i], INPUT); delayMicroseconds(100);
      any[nAny].pin = ALL_PINS[i];
      any[nAny].idle = digitalRead(ALL_PINS[i]);
      nAny++;
    }
    if (nAny > 0) {
      for (int i = 0; i < N_ALL && busyHit < 0 && !timedOut; i++) {
        for (int j = 0; j < N_ALL && busyHit < 0 && !timedOut; j++) {
          if (j == i) continue;
          for (int k = 0; k < N_ALL && busyHit < 0 && !timedOut; k++) {
            if (k == i || k == j) continue;
            for (int l = 0; l < N_ALL; l++) {
              ledTick();
              if (millis() - t0 > SCAN_CAP_MS) { timedOut = true; break; }
              pins[0]=ALL_PINS[i]; pins[1]=ALL_PINS[j]; pins[2]=ALL_PINS[k]; pins[3]=ALL_PINS[l];
              if (attempt(pins, any, nAny, busyHit)) break;
            }
          }
        }
      }
    }
  }

  if (busyHit >= 0) {
    out.clk = pins[0]; out.din = pins[1]; out.cs = pins[2]; out.dc = pins[3];
    // rst = пин, найденный в стадии 2а2 (окно из 6); -1 — нашли без аппаратного
    // RST (SWRESET достаточен, панель не спала). GxEPD2 с rst=-1 сам не
    // драйверит резет.
    out.rst = rstFound;
    out.busy = busyHit;
  }
  releaseAllPins();
  return out;
}

// свободный пин-заглушка для MISO: nrfx_spim конфигурирует MISO как ВХОД
// последним, поэтому подставлять найденный пин нельзя — шина умрёт (К1).
// общий список свободных пинов (для сканеров другой периферии, напр. стика)
const int8_t* lufreeFreePins(int &count) {
  count = N_ALL;
  return ALL_PINS;
}

int8_t ePaperStubMiso(const EPaperPins &p) {
  const int8_t used[6] = {p.clk, p.din, p.cs, p.dc, p.rst, p.busy};
  for (int i = 0; i < N_ALL; i++) {
    int8_t q = ALL_PINS[i];
    if (q < 0) continue;
    bool in = false;
    for (int j = 0; j < 6; j++) if (used[j] == q) { in = true; break; }
    if (!in) return q;
  }
  return -1;
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
  int8_t rst = p.rst;   // cfg со стадии 2а2: ping обязан пульсировать RST так же,
                        // как скан, иначе проснувшуюся панель не проверить
  if (attempt(pins, arr, 1, hit, rst) && hit == p.busy) {
    // вернуть сигнальные пины в нейтраль до инициализации SPI
    pinMode(p.clk, INPUT_PULLUP); pinMode(p.din, INPUT_PULLUP);
    pinMode(p.cs, INPUT_PULLUP);  pinMode(p.dc, INPUT_PULLUP);
    if (rst >= 0) pinMode(rst, INPUT_PULLUP);
    return true;
  }
  return false;
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
