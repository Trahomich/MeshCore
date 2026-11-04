// LuFree: калибровка 5-позиционного стика — реализация (см. StickScan.h).
//
// Все ожидания ограничены таймаутами: калибровка не может повесить загрузку
// (максимум ~90 с в самом плохом случае, BLE/радио к этому моменту уже
// подняты — вызов идёт после radio_init).

#include <Arduino.h>
#include <new>
#include "StickScan.h"
#include "EPaperScan.h"
#include <Adafruit_LittleFS.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/MomentaryButton.h>
#include "target.h"   // PIN_USER_BTN (пин-заглушка при неудаче)

using namespace Adafruit_LittleFS_Namespace;

extern Adafruit_LittleFS InternalFS;   // ядро Adafruit nRF52

// ---------- кнопки (объявлены ссылками в promicro/target.h под LUFREE_STICK) ----------

alignas(4) static uint8_t s_btns[sizeof(MomentaryButton) * 6];
static MomentaryButton* const s_btn_user   = reinterpret_cast<MomentaryButton*>(s_btns + 0 * sizeof(MomentaryButton));
static MomentaryButton* const s_btn_up     = reinterpret_cast<MomentaryButton*>(s_btns + 1 * sizeof(MomentaryButton));
static MomentaryButton* const s_btn_down   = reinterpret_cast<MomentaryButton*>(s_btns + 2 * sizeof(MomentaryButton));
static MomentaryButton* const s_btn_left   = reinterpret_cast<MomentaryButton*>(s_btns + 3 * sizeof(MomentaryButton));
static MomentaryButton* const s_btn_right  = reinterpret_cast<MomentaryButton*>(s_btns + 4 * sizeof(MomentaryButton));
static MomentaryButton* const s_btn_back   = reinterpret_cast<MomentaryButton*>(s_btns + 5 * sizeof(MomentaryButton));

MomentaryButton& user_btn      = *s_btn_user;    // стик OK  -> KEY_ENTER
MomentaryButton& joystick_up   = *s_btn_up;      //          -> KEY_UP
MomentaryButton& joystick_down = *s_btn_down;    //          -> KEY_DOWN
MomentaryButton& joystick_left = *s_btn_left;    //          -> KEY_LEFT (+ back_btn)
MomentaryButton& joystick_right= *s_btn_right;   //          -> KEY_RIGHT
MomentaryButton& back_btn      = *s_btn_back;    // пин LEFT, тройной клик -> KEY_SELECT

// конструктор со «безопасным» пином (уровень покоя, событий не будет)
static void constructButton(MomentaryButton *b, int8_t pin, bool multiclick) {
  new (b) MomentaryButton(pin, 1000, true, true, multiclick);
}

// ---------- persistence ----------

StickPins lufreeLoadStick() {
  StickPins out = {-1, -1, -1, -1, -1, 1};
  InternalFS.begin();
  File f(InternalFS.open("/stick.cfg", FILE_O_READ));
  if (f) {
    uint8_t buf[8];
    size_t n = f.read(buf, sizeof(buf));
    f.close();
    if (n == 8 && buf[0] == 0x4C && buf[1] == 0x53) {
      out.up = buf[2]; out.down = buf[3]; out.left = buf[4];
      out.right = buf[5]; out.ok = buf[6]; out.activeLow = buf[7] ? 1 : 0;
    }
  }
  return out;
}

void lufreeSaveStick(const StickPins &s) {
  InternalFS.begin();
  File f(InternalFS.open("/stick.cfg", FILE_O_WRITE));
  if (f) {
    uint8_t buf[8] = {0x4C, 0x53, (uint8_t)s.up, (uint8_t)s.down, (uint8_t)s.left,
                      (uint8_t)s.right, (uint8_t)s.ok, s.activeLow ? 1 : 0};
    f.write(buf, sizeof(buf));
    f.close();
  }
}

// ---------- калибровка ----------

namespace {

const int SAMPLE_MS = 4;          // период опроса линий
const int DEBOUNCE_MS = 48;       // устойчивое нажатие
const uint32_t PHASE_MS = 8000;   // таймаут одной полярности на направление
const uint32_t RELEASE_MS = 5000; // максимум ожидания отпускания

void led(bool on) {
#ifdef PIN_LED
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#endif
}

// мигнуть LED n раз (подсказка направления, когда экрана нет)
void ledBlink(int n) {
  for (int i = 0; i < n; i++) {
    led(true); delay(140); led(false); delay(160);
  }
  delay(400);
}

// вернуть пины-кандидаты в нейтральное состояние
void neutralize(const int8_t *pins, int n) {
  for (int i = 0; i < n; i++) if (pins[i] >= 0) pinMode(pins[i], INPUT);
}

// подсказка на экране (e-paper обновляется медленно — рисуем один раз)
void promptScreen(DisplayDriver *disp, int step, int total, const char *name, const char *sym) {
  if (!disp) return;
  disp->startFrame();
  disp->clear();
  disp->setTextSize(2);
  disp->drawTextCentered(disp->width() / 2, 10, "STICK SETUP");
  disp->setTextSize(3);
  disp->drawTextCentered(disp->width() / 2, disp->height() / 3, sym);
  disp->setTextSize(2);
  disp->drawTextCentered(disp->width() / 2, disp->height() / 2 + 10, name);
  char line[24];
  snprintf(line, sizeof(line), "%d / %d", step, total);
  disp->drawTextCentered(disp->width() / 2, disp->height() - 30, line);
  disp->endFrame();
}

// ждём устойчивое нажатие любой линии; phase=0 -> подтяжка вверх (нажатие LOW),
// phase=1 -> подтяжка вниз (нажатие HIGH). Возвращает пин или -1 по таймауту.
int8_t watchPress(const int8_t *cand, int nc, int phase, uint32_t window_ms) {
  uint8_t idle = phase == 0 ? HIGH : LOW;
  for (int i = 0; i < nc; i++) pinMode(cand[i], phase == 0 ? INPUT_PULLUP : INPUT_PULLDOWN);
  int hold[16] = {0};
  uint32_t t0 = millis();
  int8_t hit = -1;
  while (millis() - t0 < window_ms) {
    delay(SAMPLE_MS);
    for (int i = 0; i < nc; i++) {
      if (digitalRead(cand[i]) != idle) hold[i] += SAMPLE_MS;
      else hold[i] = 0;
      if (hold[i] >= DEBOUNCE_MS) { hit = cand[i]; break; }
    }
    if (hit >= 0) break;
  }
  return hit;
}

// ждём отпускания (или таймаут), затем вернём пины в нейтраль
void waitRelease(int8_t pin, int phase) {
  uint8_t idle = phase == 0 ? HIGH : LOW;
  uint32_t t0 = millis();
  while (millis() - t0 < RELEASE_MS) {
    if (digitalRead(pin) == idle) { delay(60); if (digitalRead(pin) == idle) break; }
    delay(4);
  }
  pinMode(pin, INPUT);
}

} // namespace

StickPins stickCalibrate(DisplayDriver *disp) {
  StickPins out = {-1, -1, -1, -1, -1, 1};

  // кандидаты: свободные пины минус занятые экраном (включая MISO-заглушку)
  EPaperPins ep = lufreeLoadPins();
  int8_t stub = ePaperStubMiso(ep);
  int nFree = 0;
  const int8_t *freePins = lufreeFreePins(nFree);
  int8_t cand[16];
  int nc = 0;
  for (int i = 0; i < nFree && nc < 16; i++) {
    int8_t p = freePins[i];
    if (p == ep.clk || p == ep.din || p == ep.cs || p == ep.dc || p == ep.rst ||
        p == ep.busy || p == stub) continue;
    cand[nc++] = p;
  }
  if (nc < 5) return out;   // свободных пинов меньше, чем линий стика

#ifdef PIN_LED
  pinMode(PIN_LED, OUTPUT);
#endif

  static const char *names[5] = {"UP", "DOWN", "LEFT", "RIGHT", "OK (press)"};
  static const char *syms[5] = {"^", "v", "<-", "->", "o"};
  int8_t *slots[5] = {&out.up, &out.down, &out.left, &out.right, &out.ok};

  for (int step = 0; step < 5; step++) {
    promptScreen(disp, step + 1, 5, names[step], syms[step]);
    //candidates: скопировать оставшиеся (кроме уже занятых предыдущими шагами)
    int8_t pool[16];
    int np = 0;
    for (int i = 0; i < nc; i++) {
      bool taken = false;
      for (int j = 0; j < step; j++) if (*slots[j] == cand[i]) { taken = true; break; }
      if (!taken) pool[np++] = cand[i];
    }
    int8_t hit = -1; int usedPhase = 0;
    for (int phase = 0; phase < 2 && hit < 0; phase++) {
      usedPhase = phase;
      ledBlink(step + 1);      // подсказка направления (LED, когда экрана нет)
      led(true);               // LED горит, пока ждём нажатие
      hit = watchPress(pool, np, phase, PHASE_MS);
      led(false);
      if (hit < 0) delay(600); // пауза перед второй полярностью
    }
    if (hit >= 0) {
      // фаза 0 (INPUT_PULLUP): нажатие = LOW; фаза 1 (PULLDOWN): нажатие = HIGH
      out.activeLow = (usedPhase == 0) ? 1 : 0;
      *slots[step] = hit;
      waitRelease(hit, usedPhase);
      led(true); delay(120); led(false);
    } else {
      // не нашли направление — пины в нейтраль, стик целиком отключаем
      neutralize(cand, nc);
      if (disp) {
        disp->startFrame(); disp->clear();
        disp->setTextSize(2);
        disp->drawTextCentered(disp->width() / 2, disp->height() / 2, "STICK: not found");
        disp->endFrame();
      }
      return StickPins{-1, -1, -1, -1, -1, 1};
    }
  }
  neutralize(cand, nc);  // MomentaryButton::begin() настроит свои подтяжки заново
  return out;
}

// ---------- высокоуровневая инициализация (вызов из main.cpp) ----------

// зажата ли какая-нибудь линия стика из сохранённого конфига (>= 3 с)?
static bool stickHeld(const StickPins &s) {
  int8_t pins[5] = {s.up, s.down, s.left, s.right, s.ok};
  pinMode(pins[0], s.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  pinMode(pins[1], s.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  pinMode(pins[2], s.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  pinMode(pins[3], s.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  pinMode(pins[4], s.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  uint8_t idle = s.activeLow ? HIGH : LOW;
  bool any = false;
  for (int i = 0; i < 5; i++) if (digitalRead(pins[i]) != idle) any = true;
  if (!any) return false;
  delay(3000);
  for (int i = 0; i < 5; i++) if (digitalRead(pins[i]) != idle) return true;
  return false;
}

void lufreeStickSetup(DisplayDriver *disp) {
  StickPins s = lufreeLoadStick();
  if (s.valid() && !stickHeld(s)) {
    // быстрый путь: конфиг есть, линии в покое — сразу конструируем кнопки
  } else {
    s = stickCalibrate(disp);
    if (s.valid()) lufreeSaveStick(s);
  }
  if (s.valid()) {
    bool rev = s.activeLow;    // нажатие LOW -> reverse (активен LOW)
    new (s_btn_user)   MomentaryButton(s.ok,    1000, rev, true, false);
    new (s_btn_up)     MomentaryButton(s.up,    1000, rev, true, false);
    new (s_btn_down)   MomentaryButton(s.down,  1000, rev, true, false);
    new (s_btn_left)   MomentaryButton(s.left,  1000, rev, true, false);
    new (s_btn_right)  MomentaryButton(s.right, 1000, rev, true, false);
    new (s_btn_back)   MomentaryButton(s.left,  1000, rev, true, true);   // multiclick: тройной клик = меню
  } else {
    // неудача: кнопки на безопасный пин PIN_USER_BTN (уровень покоя,
    // событий не будет) — прошивка работает как без стика.
    constructButton(s_btn_user, PIN_USER_BTN, false);
    constructButton(s_btn_up, PIN_USER_BTN, false);
    constructButton(s_btn_down, PIN_USER_BTN, false);
    constructButton(s_btn_left, PIN_USER_BTN, false);
    constructButton(s_btn_right, PIN_USER_BTN, false);
    constructButton(s_btn_back, PIN_USER_BTN, true);
  }
}
