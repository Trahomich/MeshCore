# Исследование: система вариантов и CI в MeshCore

Факты из репозитория `workspace/MeshCore` (клон git@github.com:Trahomich/MeshCore.git).
Цель документа: дать следующей задаче всё необходимое для добавления варианта прошивки **LuFree**
(Heltec HT-RA62: nRF52840 + SX1262, e-paper WeAct 2.9" 128x296, 5-позиционный стик, LED) без чтения репозитория целиком.

## 1. Как устроен вариант прошивки

Репозиторий — один PlatformIO-проект. Сборочные окружения (`[env:...]`) не лежат в корневом
`platformio.ini` — они разнесены по `variants/<имя>/platformio.ini`, которые подхватываются строкой в
корневом файле:

- `platformio.ini`, блок `[platformio]`: `extra_configs = variants/*/platformio.ini` (+ `platformio.local.ini`).
- Корневой `platformio.ini` содержит только базовые секции: `arduino_base`, `esp32_base`, `nrf52_base`,
  `rp2040_base`, `stm32_base`, `sensor_base`, `env:native` (тесты).

### Состав папки варианта (пример `variants/wio_wm1110/` — nRF52840 + LR1110)

| Файл | Назначение |
|---|---|
| `platformio.ini` | Базовая секция (название = имени варианта без `env:`) + окружения `[env:<имя>_<роль>]`: `_companion_radio_ble`, `_companion_radio_usb`, `_repeater`, `_room_server`, `_kiss_modem` |
| `target.h` / `target.cpp` | Объекты `board`, `radio_driver`, `rtc_clock`, `sensors`, (`display`, `user_btn` если есть экран); функции `radio_init()`, `radio_new_identity()` |
| `variant.cpp` / `variant.h` | nRF52-специфика уровня Adafruit-ядра: номера пинов (`PIN_SERIAL1_RX`, `PIN_WIRE_SDA`, `PINS_COUNT` …), тактирование |
| `<Board>.cpp/.h` (необязательно) | Класс платы, наследник `NRF52Board` (или ESP32Board и т.п.), см. `src/helpers/NRF52Board.h` |
| `pins_arduino.h` | ESP32-варианты: маппинг пинов для ядра Arduino |

Связь с `boards/`: nRF52-варианты берут готовый board-json (например
`board = seeed-xiao-afruitnrf52-nrf52840` — xiao-подобные платы nRF52840 с S140 v7; `board_build.ldscript = boards/nrf52840_s140_v7.ld`).
Свежие board-json для новых плат кладут в `boards/` и указывают `board = <имя_файла_без_.json>`.
Пиннинг радиочасти и периферии задаётся `-D P_LORA_*=`, `-D DISP_*=`, `-D PIN_USER_BTN=` и т.п. в build_flags.

### Шаблон окружения companion (nRF52, пример `variants/wio_wm1110/platformio.ini` и `variants/lilygo_techo/platformio.ini`)

Компаньон BLE: `extends = <базовая_секция>` + `board_build.ldscript = boards/nrf52840_s140_v7_extrafs.ld`,
`board_upload.maximum_size = 708608`, флаги `-D MAX_CONTACTS=350 -D MAX_GROUP_CHANNELS=40 -D BLE_PIN_CODE=123456 -D QSPIFLASH=1 -D OFFLINE_QUEUE_SIZE=256`,
`-I examples/companion_radio/ui-new`, build_src_filter добавляет
`+<helpers/nrf52/SerialBLEInterface.cpp>`, `+<../examples/companion_radio/*.cpp>`, `+<../examples/companion_radio/ui-new/*.cpp>`,
lib_deps + `densaugeo/base64 @ ~1.4.0`.

Экран подключается макросом `-D DISPLAY_CLASS=...` + добавлением `helpers/ui/<Класс>Display.cpp` в build_src_filter.
Для GxEPD2-e-ink (см. `variants/wio-tracker-l1-eink/platformio.ini` и `src/helpers/ui/GxEPDDisplay.h`):
`-D DISPLAY_CLASS=GxEPDDisplay`, `-D EINK_DISPLAY_MODEL=GxEPD2_213_B74` (выбор контроллера GxEPD2), `-D EINK_SCALE_X/Y`, `-D EINK_X/Y_OFFSET`, `-D DISPLAY_ROTATION=1`,
lib_deps: `adafruit/Adafruit GFX Library @ ^1.12.1`, `zinggjm/GxEPD2 @ 1.6.2`. Пины экрана: `PIN_DISPLAY_CS/DC/RST/BUSY` (или `DISP_CS/DC/RST/BUSY`, если модель не задана).

Роль прошивок суффиксами `_companion_radio_usb`, `_companion_radio_ble`, `_repeater`, `_room_server`, `_kiss_modem` — это важно для build.sh (см. ниже).

### Что нужно для нового варианта LuFree (шаги)

1. Папка `variants/lufree/` с `platformio.ini` (+ при необходимости `target.cpp/h`, `variant.cpp/h`, `LuFreeBoard.cpp/h`).
2. Базовая секция `[lufree]`: `extends = nrf52_base`; `board = seeed-xiao-afruitnrf52-nrf52840` (или собственный `boards/lufree.json` по образцу `boards/promicro_nrf52840.json`); линкер `boards/nrf52840_s140_v7.ld`.
3. Радио SX1262, как у nRF52840+SX1262-вариантов `variants/lilygo_techo` и `variants/mesh_pocket`: `-D USE_SX1262 -D RADIO_CLASS=CustomSX1262 -D WRAPPER_CLASS=CustomSX1262Wrapper`, `-D P_LORA_DIO_1/NSS/RESET/BUSY/SCLK/MOSI/MISO=...`, `-D SX126X_DIO2_AS_RF_SWITCH` и др. (уточнить по маркировке платы; если модуль окажется LR1110 — образец `variants/wio_wm1110` с CustomLR1110, но по спецификации Heltec HT-RA62 — SX1262).
4. Экран: `-D DISPLAY_CLASS=GxEPDDisplay`, `-D EINK_DISPLAY_MODEL=GxEPD2_290_...` (модель для 2.9" 128x296, выбор — задача исследования e-paper #3), пины `PIN_DISPLAY_*`.
5. Окружение `[env:lufree_companion_radio_ble]` (+ при желании `_usb`) по шаблону T-Echo/wm1110 выше.
6. Env-имя должно содержать суффикс `_companion_radio_ble`/`_usb`, чтобы попасть в `build.sh build-companion-firmwares`, либо вызывать `build.sh build-firmware lufree_companion_radio_ble` явно.

## 2. CI: как отключить автосборку всех прошивок, кроме LuFree

### Как устроена сборка в CI

Все три firmware-workflow устроены одинаково и запускают `build.sh`:

- `.github/workflows/build-companion-firmwares.yml`: триггеры `workflow_dispatch` + `push: tags: ['companion-*']`; шаг `run: /usr/bin/env bash build.sh build-companion-firmwares`; артефакт `out/` + draft-release по тегу.
- `.github/workflows/build-repeater-firmwares.yml`: то же, теги `repeater-*`, `build.sh build-repeater-firmwares`.
- `.github/workflows/build-room-server-firmwares.yml`: то же, теги `room-server-*`, `build.sh build-room-server-firmwares`.
- `.github/workflows/pr-build-check.yml`: matrix-список 11 окружений (Heltec_v3, RAK_4631, PicoW, wio-e5-mini, Tlora_C6, wio_wm1110_repeater, Tbeam_SX1276); триггеры push/PR в main/dev по путям `src/**, examples/**, variants/**, platformio.ini, сам workflow`. Сборка: `pio run -e ${{ matrix.environment }}`.
- `.github/workflows/run-unit-tests.yml`: `pio test -e native` на каждый push/PR — к прошивкам отношения не имеет.
- `.github/workflows/github-pages.yml`: mkdocs-документация на push в main — к прошивкам не относится.
- Общий сетап: `.github/actions/setup-build-environment` (pip install platformio + извлечение версии из тега `room-server-v1.2.3` → `v1.2.3`).

`build.sh` не имеет статического списка: он вызывает `pio project config`, берёт **все** env из `variants/*/platformio.ini` и строит всё по суффиксу:
`build_all_firmwares_by_suffix "_companion_radio_usb"` + `"_companion_radio_ble"` (иначе `_repeater`, `_room_server`).
Т.е. **пока в репозитории лежат другие варианты, любой вызов `build-companion-firmwares` соберёт и их**.

### Что именно менять (точные места)

Вариант A (рекомендуемый, минимальные правки — LuFree остаётся единственной автособираемой прошивкой):

1. `build.sh`, функция `build_companion_firmwares()` (строки 211–227): закомментировать/убрать `build_all_firmwares_by_suffix "_companion_radio_usb"` и `"_companion_radio_ble"` и вместо них — явный список:
   ```bash
   build_firmware "lufree_companion_radio_ble"
   # build_firmware "lufree_companion_radio_usb"   # при наличии env
   ```
   Аналогично `build_repeater_firmwares()` и `build_room_server_firmwares()` (там у LuFree ролей нет — можно оставить тело пустым или `:;` с комментарием «autobuild disabled, LuFree only»).
2. `build-companion-firmwares.yml`: триггеры оставить как есть (`workflow_dispatch` + теги `companion-*`) — сборка пойдёт только для LuFree. При желании добавить срабатывание и на push в main (тогда артефакт будет обновляться без тега; Release-шаг уже ограничен `if: startsWith(github.ref, 'refs/tags/')`).
3. `build-repeater-firmwares.yml` и `build-room-server-firmwares.yml`: отключить автосборку полностью — убрать блоки `on: push: tags: ...`, оставив только `workflow_dispatch:` (или удалить файлы; сохранение с `workflow_dispatch` мягче).
4. `pr-build-check.yml`: в `jobs.build.strategy.matrix.environment` заменить весь список на единственную строку `- lufree_companion_radio_ble` (так проверка сборки на каждый push/PR останется только для LuFree).
5. `run-unit-tests.yml` и `github-pages.yml` не трогать (не прошивки).

Вариант B (радикальный): удалить `variants/*` других плат и три build-workflow, оставив один workflow, собирающий `lufree` — не рекомендуется: ломает историю/совместимость апстрима и заметно усложняет поддержку.

## 3. Локальная сборка

- PlatformIO CLI установлен в workspace-окружении: `~/.local/bin/pio` (установлен через `uv tool install platformio`; в PATH его нет — добавлять `export PATH="$HOME/.local/bin:$PATH"` или вызывать по полному пути). Сеть доступна, пакеты/платформы качаются.
- Сборка одного варианта: `cd MeshCore && pio run -e <env>` (например `pio run -e lufree_companion_radio_ble`). Полный артефакт — через `sh build.sh build-firmware <env>` (нужен `export FIRMWARE_VERSION=v0.0.1`), он же делает `.uf2` для nRF52 (`bin/uf2conv/uf2conv.py`, флаг `0xADA52840`) и кладёт в `out/`.
- Первая сборка качает платформу `nordicnrf52`, форк `framework-arduinoadafruitnrf52` (meshcore-dev) и библиотеки — несколько сотен МБ и десятки минут; далее кэшируется в `~/.platformio`.
- Валидационная сборка `wio_wm1110_repeater` (nRF52840) выполнена локально успешно: `pio run -e wio_wm1110_repeater` → SUCCESS за ~4.5 мин (RAM 12.4%, Flash 51.1%), включая скачивание платформы nordicnrf52 и библиотек. Значит локальная сборка нового варианта LuFree полностью реалистична.
- Вывод: локальная сборка реалистична и её стоит использовать для проверки перед CI; CI остаётся источником релизных артефактов.

## Приложение: полезные факты для LuFree

- Плата Heltec HT-RA62 = nRF52840 + SX1262 (по спецификации heltec.org/project/ht-ra62; RF-чип модуля — SX1262). Радиочасть: `USE_SX1262` / `CustomSX1262` / `CustomSX1262Wrapper`, флаги `SX126X_*` — образцы `variants/lilygo_techo`, `variants/mesh_pocket`. («WeAct Studio» из ответа пользователя относится к экрану, не к плате.) LR1110 — только если маркировка платы его покажет (образец тогда `variants/wio_wm1110`).
- nRF52-компаньон с экраном уже существует: `variants/lilygo_techo` (T-Echo, e-ink + GxEPDDisplay) и `variants/wio-tracker-l1-eink` (env только repeater, но показывает набор `EINK_*`-макросов).
- `ikoka_stick_nrf` — пример nRF52-варианта без собственной board-платы (использует `seeed-xiao-afruitnrf52-nrf52840`).
- 5-позиционный стик: в MeshCore нет готового драйвера 5-way stick; ввод — `PIN_USER_BTN` + `src/helpers/ui/MomentaryButton.cpp`. Стик-навигация — отдельная задача (исследование #4).
- Итоговое имя env, упомянутое в задаче планирования: `lufree` (минимум `lufree_companion_radio_ble`).
