# Исследование: ввод (кнопки/стик) и nRF52840 в MeshCore

Факты из репозитория `MeshCore/` (коммит 07a3ca9e). Для задачи по стику и LED LuFree.

## 1. Устройство ввода companion-прошивки

- Ввод реализован в `examples/companion_radio/ui-new/UITask.cpp` (и `ui-tiny/`) в `UITask::loop()` (ui-new: строки ~716–755). Кнопки — глобальные `MomentaryButton` из `variants/<имя>/target.cpp`, объявленные `extern` в `variants/<имя>/target.h`.
- Класс `MomentaryButton` — `src/helpers/ui/MomentaryButton.{h,cpp}`:
  - конструктор `MomentaryButton(pin, long_press_millis, reverse, pulldownup, multiclick)`;
  - `reverse=true` — кнопка активна LOW; `pulldownup=true` — `pinMode(INPUT_PULLUP/INPUT_PULLDOWN)` (см. `begin()`); для стика на GPIO к GND берут `(pin, 1000, true, false, false)` (как в `variants/gat562_mesh_tracker_pro/target.cpp`);
  - `check()` возвращает `BUTTON_EVENT_CLICK / LONG_PRESS / DOUBLE_CLICK / TRIPLE_CLICK / NONE`.
- Коды клавиш UI — `src/helpers/ui/UIScreen.h`: `KEY_UP 0xB5`, `KEY_DOWN 0xB6`, `KEY_LEFT 0xB4`, `KEY_RIGHT 0xB7`, `KEY_ENTER 13`, `KEY_SELECT 10`, `KEY_CANCEL 27`. Экраны получают их в `handleInput(char c)`.
- Режим джойстика включается define `UI_HAS_JOYSTICK=1` в `platformio.ini` варианта (примеры: `variants/gat562_mesh_tracker_pro/platformio.ini:12`, `variants/wio-tracker-l1-eink/platformio.ini:53`).

## 2. Что умеет штатный UI_HAS_JOYSTICK и чего не хватает

В `UITask::loop()` (ui-new, ui-tiny) при `UI_HAS_JOYSTICK` читаются только **3** кнопки + back:
- `user_btn` → `KEY_ENTER` (click) / long-press;
- `joystick_left` → `KEY_LEFT`; `joystick_right` → `KEY_RIGHT`;
- `back_btn` (тройной клик) → `KEY_SELECT`.

**Вверх/вниз (`joystick_up`/`joystick_down` → `KEY_UP`/`KEY_DOWN`) в общем коде НЕ читаются**, хотя `KEY_UP/KEY_DOWN` в UI есть (используются в списках). В `variants/wio-tracker-l1/variant.h:35–36` defines `JOYSTICK_UP/JOYSTICK_DOWN` объявлены, но объекты и обработка не подключены.

⇒ Для полной 5-позиционной навигации нужно минимально расширить общий файл `examples/companion_radio/ui-new/UITask.cpp` (+ объявление extern/объектов в `variants/lufree/target.{h,cpp}`): добавить блоки `#if defined(JOYSTICK_UP)` / `#if defined(JOYSTICK_DOWN)` по образцу `joystick_left/right` (click → `checkDisplayOn(KEY_UP)` и т.д., long-press → `handleLongPress(...)`). Изменение общего файла — согласовать в обсуждении задачи (контракт, раздел 2). Чтобы не задеть чужие варианты, новые блоки guarded: сработают только у варианта, объявившего `JOYSTICK_UP/DOWN` **и** создавшего объекты `joystick_up/joystick_down` (у wio-tracker-l1 объекты не созданы, поэтому там guarded-код не включать в компиляцию: либо не определять для него макрос-объект, либо обернуть дополнительно своим define, например `UI_JOYSTICK_5WAY=1` — рекомендуемый вариант).

## 3. Маппинг для LuFree (5 стик)

| Кнопка | Пин (target.h) | Действие |
|---|---|---|
| UP | `PIN_STICK_UP` | `KEY_UP` — навигация вверх |
| DOWN | `PIN_STICK_DOWN` | `KEY_DOWN` — навигация вниз |
| LEFT | `PIN_STICK_LEFT` | `KEY_LEFT` — назад |
| RIGHT | `PIN_STICK_RIGHT` | `KEY_RIGHT` — вперёд |
| OK (нажатие) | `PIN_STICK_OK` | `KEY_ENTER` (click), long-press — как у `user_btn` |

Объекты: `MomentaryButton joystick_up(PIN_STICK_UP, 1000, true, false, false);` и аналогично (активный уровень LOW, без подтяжки — подтяжку платы уточнить; при необходимости `pulldownup=true`).

## 4. LED-оповещение

- Штатный механизм: в `examples/companion_radio/ui-new/UITask.cpp` (`userLedHandler()`, ~строка 654) при `#ifdef PIN_STATUS_LED` LED мигает, пока есть непрочитанные сообщения (`_msgcount > 0`). Нужно только определить `PIN_STATUS_LED` и `LED_STATE_ON` для варианта; объект `RefCountedDigitalPin`/`pinMode` — см. строки 14 и 654.
- Для LuFree: `#define PIN_STATUS_LED PIN_LED_NOTIFY` и `LED_STATE_ON` по схеме платы (по умолчанию 1). Дополнительно мигать не нужно — штатной индикации достаточно.

## 5. nRF52840 / board JSON для HT-RA62

- nRF52840-варианты строятся от секции `nrf52_base` корневого `platformio.ini`, с `board = <имя>` и `boards/*.json` (пример: `variants/wio-tracker-l1-eink/platformio.ini`: `board = seeed-wio-tracker-l1`, ldscript `boards/nrf52840_s140_v7.ld` (+`_extrafs` для companion с BLE), SoftDevice s140 7.3.0 из `lib/nrf52/`).
- Готового board JSON для Heltec HT-RA62 в репозитории НЕТ (поиск ra62 — пусто). Пины варианта задаются своим `variant.h`/`target.h` (пример кастомного набора пинов поверх стандартной платы: `variants/gat562_mesh_tracker_pro` — extends rak4631-плату, но свои `variant.h`, `target.h`).
- Рекомендация для LuFree: взять за основу существующий nRF52840 board JSON (например `boards/seeed-wio-tracker-l1.json` или ближайший по флешу), скопировать в `boards/lufree.json`, поменять имя/описание; все реальные пины (радио, экран, стик, LED) — в `variants/lufree/target.h`.
- Радио HT-RA62 (Heltec HT-RA62, см. heltec.org/project/ht-ra62) — **Semtech SX1262**, НЕ LR1110 («weact studio» в ответе пользователя относилось к модулю e-paper, не к плате). В MeshCore обвязка: `RADIO_CLASS=CustomSX1262`, `WRAPPER_CLASS=CustomSX1262Wrapper`, флаги `SX126X_DIO2_AS_RF_SWITCH`, `SX126X_DIO3_TCXO_VOLTAGE`, пины `P_LORA_*` (образцы nRF52840+SX1262: `variants/lilygo_techo`, `variants/mesh_pocket`, `variants/wio-tracker-l1-eink/platformio.ini`). Если по маркировке модуля на конкретной плате окажется LR1110 — альтернатива `CustomLR1110`/`CustomLR1110Wrapper` (образец `variants/wio_wm1110`), но основная ветка для LuFree — SX1262.
- Пины Heltec HT-RA62 (nRF52840 + SX1262) в репозитории не описаны — значения-заготовки с пометкой «сверить с платой».

## 6. Где смотреть образцы

- Джойстик-вариант: `variants/gat562_mesh_tracker_pro/` (target.cpp — 4 MomentaryButton), `variants/wio-tracker-l1/` (variant.h — JOYSTICK_UP/DOWN/LEFT/RIGHT/PRESS).
- e-ink + joystick + nRF52840 companion: `variants/wio-tracker-l1-eink/platformio.ini` — ближайший образец конфигурации для LuFree.
