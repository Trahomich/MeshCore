# Исследование: e-paper в MeshCore и подключение экрана 2.9" 128x296 (WeAct) в LuFree

Проверено по репозиторию `MeshCore/` (клон Trahomich/MeshCore) и исходникам библиотеки GxEPD2 **1.6.2** (именно эта версия подключена во всех вариантах MeshCore). Только факты и ссылки на код; реализация — в задачах #5/#6.

## 1. Как MeshCore подключает e-paper дисплей

Универсальный интерфейс дисплея — абстрактный класс **`src/helpers/ui/DisplayDriver.h`** (`isOn/isEink/turnOn/turnOff/clear/startFrame/setTextSize/setColor/setCursor/print/fillRect/drawRect/drawXbm/getTextWidth/endFrame`). Вариант прошивки выбирает конкретный класс через `-D DISPLAY_CLASS=...` в своём `variants/<name>/platformio.ini`.

Для GxEPD-панелей используется готовый драйвер **`src/helpers/ui/GxEPDDisplay.h` / `.cpp`** — обёртка над `GxEPD2_BW<MODEL, MODEL::HEIGHT>`. Он уже реализует всё: CRC32-дедупликацию кадров (лишний `endFrame()` не обновляет панель), частичное обновление (`setPartialWindow(0,0,w,h)` в `begin()`, `display.display(true)` в `endFrame()`), инверсию цветов, масштабирование шрифтов (`FreeSans9pt/FreeSansBold12pt/FreeSans18pt`), сон панели через GxEPD2 (`powerOff`/`hibernate` внутри библиотеки). **Свой драйвер писать не нужно.**

### Контракт GxEPDDisplay (все #define из его кода)

| Define | Назначение | Где задаётся |
|---|---|---|
| `EINK_DISPLAY_MODEL` | Класс панели GxEPD2 (например `GxEPD2_290_T94`) | build_flags варианта (или target.h) |
| `EINK_SCALE_X`, `EINK_SCALE_Y`, `EINK_X_OFFSET`, `EINK_Y_OFFSET` | Масштаб/смещение логического холста 128x128 на физическую панель | build_flags варианта |
| `DISPLAY_ROTATION` | Поворот панели, по умолчанию 3 (см. `GxEPDDisplay.cpp`) | build_flags варианта |
| `PIN_DISPLAY_CS`, `PIN_DISPLAY_DC`, `PIN_DISPLAY_RST`, `PIN_DISPLAY_BUSY` | Пины панели, подставляются в конструктор GxEPD2 | `variant.h` варианта |
| `PIN_DISPLAY_SCLK`, `PIN_DISPLAY_MISO`, `PIN_DISPLAY_MOSI` | Пины SPI (используются только на ESP32, см. ниже) | `variant.h` варианта |
| `DISPLAY_CLASS=GxEPDDisplay` | Выбор драйвера | build_flags варианта |
| lib_deps | `zinggjm/GxEPD2 @ 1.6.2`, `bakercp/CRC32 @ ^2.0.0` (+ Adafruit GFX тянется зависимостями; wio-tracker-l1-eink для надёжности указывает и `adafruit/Adafruit GFX Library @ ^1.12.1`) | `variants/<name>/platformio.ini` |
| build_src_filter | `+<helpers/ui/GxEPDDisplay.cpp>` | `variants/<name>/platformio.ini` |

Примеры-образцы в репозитории:
- **nRF52840 + GxEPD2**: `variants/mesh_pocket/` (Heltec Mesh Pocket, 2.13"), `variants/wio-tracker-l1-eink/` (см. `variants/wio-tracker-l1/variant.h`, «EInk on SPI1»), `variants/lilygo_techo/`.
- **ESP32 + GxEPD2**: `variants/thinknode_m5/`, `variants/lilygo_techo_lite/`.
- Отдельный случай — `variants/heltec_e290/`: драйвер `src/helpers/ui/E290Display.*` на библиотеке `heltec-eink-modules` (панель Heltec E290, 296x128, логический холст без масштабирования). Он завязан на модули Heltec, WeAct-модуль им не поддерживается — **не использовать**, упомянут для полноты.

## 2. SPI на nRF52840 — важная особенность GxEPDDisplay

В `GxEPDDisplay.cpp`:
- на ESP32 создаётся свой `SPIClass SPI1 = SPIClass(FSPI)` и `SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS)`;
- **на nRF52840 ветки нет**: используется глобальный `SPI1` из ядра Adafruit nRF52, и `SPI1.begin()` без аргументов — пины берутся из `variant.h`: `#define SPI_INTERFACES_COUNT 2` + `PIN_SPI1_SCK`, `PIN_SPI1_MISO`, `PIN_SPI1_MOSI` (пример: `variants/wio-tracker-l1/variant.h:105-115`, `variants/mesh_pocket/variant.h:106-118`).

Итого для LuFree (HT-RA62, nRF52840): экран вешается на **SPI1**, радио остаётся на `SPI` (SPIM0). В `variant.h` нужны:
```c
#define SPI_INTERFACES_COUNT    2
// e-paper WeAct 2.9" — сверить с платой
#define PIN_DISPLAY_CS          (...)   // чип-селект панели
#define PIN_DISPLAY_DC          (...)   // data/command
#define PIN_DISPLAY_RST         (...)   // reset
#define PIN_DISPLAY_BUSY        (...)   // busy
#define PIN_SPI1_SCK            (...)
#define PIN_SPI1_MOSI           (...)
#define PIN_SPI1_MISO           (-1)    // e-paper только пишет; -1 допустим
// GxEPD2 требует эти имена даже для неиспользуемой шины (см. mesh_pocket/variant.h)
extern const int MISO;
extern const int MOSI;
extern const int SCK;
```
Сигналы панели: `CS`, `DC`, `RST`, `BUSY` + `SCK`/`MOSI` (MISO не нужен; питание 3.3 В, GND). Скорость — 4 МГц задаёт `GxEPDDisplay::begin()` (классы GxEPD2 для 2.9" допускают до 10 МГц).

## 3. Панель 2.9" 128x296: готовая поддержка есть

MeshCore сам не содержит драйверов панелей 2.9" — панели берутся из GxEPD2 1.6.2. В этой версии все классы 2.9" **128x296**, BW, c частичным обновлением (проверено по исходникам `src/epd/GxEPD2_290_*.h`, `src/gdey/GxEPD2_290_GDEY029T94.h`):

| Класс GxEPD2 | Контроллер | Примечание |
|---|---|---|
| `GxEPD2_290_GDEY029T94` | **SSD1680** | Good Display GDEY029T94; быстрое частичное обновление (`hasFastPartialUpdate=true`, `useFastFullUpdate=true`); **рекомендуемый дефолт** |
| `GxEPD2_290_T94` | **SSD1680** | GDEM029T94, классический вариант |
| `GxEPD2_290_T94_V2` | **SSD1680** | Waveshare 2.9" V2 (без partial-таблиц в OTP, медленнее) |
| `GxEPD2_290_BS` | **SSD1680** | DEPG0290BS |
| `GxEPD2_290_T5D` | **UC8151D** | GDEW029T5D |
| `GxEPD2_290_M06` | **UC8151D** | GDEW029M06 |
| `GxEPD2_290_T5` | **IL0373** | GDEW029T5 |
| `GxEPD2_290` | **IL3820** | GDEH029A1, самый старый |

(Не подходят: `GxEPD2_290_GDEY029T71H` — это 128x384; `GxEPD2_290_I6FD` — спец-панель GDEW029I6FD.)

Разница контроллеров инкапсулирована внутри GxEPD2 (разные команды инициализации, LUT, **уровень BUSY**: SSD1680 — `HIGH`, UC8151D/IL0373 — `LOW`; частота до 10 МГц у всех). Для MeshCore классы взаимозаменяемы: у всех одинаковый конструктор `(cs, dc, rst, busy)` и одинаковый набор методов, который вызывает `GxEPDDisplay`. Поэтому **выбор контроллера меняется одним define**, правок кода не требуется.

## 4. Рекомендуемая конфигурация для LuFree (для задач #5/#6)

В `variants/lufree/platformio.ini` (по образцу `mesh_pocket`, но панель 2.9"):
```ini
  -D DISPLAY_CLASS=GxEPDDisplay
  -D EINK_DISPLAY_MODEL=GxEPD2_290_GDEY029T94   ; SSD1680, дефолт; альтернативы ниже
  -D EINK_SCALE_X=2.3125f      ; 296/128 — логический холст 128x128 на панель 296x128 (поворот)
  -D EINK_SCALE_Y=1.0f
  -D EINK_X_OFFSET=0
  -D EINK_Y_OFFSET=10          ; компенсация базовой линии шрифтов (как у mesh_pocket/wio)
  -D DISPLAY_ROTATION=1        ; 1 или 3 — поворот в landscape 296x128, сверить по устройству
```
Альтернативы для `EINK_DISPLAY_MODEL` (раскомментировать одну при другом контроллере модуля WeAct): `GxEPD2_290_T94` (SSD1680/Waveshare-клон), `GxEPD2_290_T5D`/`GxEPD2_290_M06` (UC8151D), `GxEPD2_290_T5` (IL0373). Все 128x296, менять только этот define (сверено по исходникам GxEPD2 1.6.2).

`lib_deps` варианта: `${nrf52_base.lib_deps}` + `zinggjm/GxEPD2 @ 1.6.2` + `bakercp/CRC32 @ ^2.0.0` (при желании `adafruit/Adafruit GFX Library @ ^1.12.1`, как в wio-tracker-l1-eink). `build_src_filter`: `+<helpers/ui/GxEPDDisplay.cpp>`.

### О WeAct Studio 2.9" 128x296
Контроллер на модуле не маркирован (по словам пользователя). Такие модули WeAct обычно повторяют Waveshare 2.9" и выпускаются в двух ревизиях: с SSD1680 (ревизия V2/V4) и с UC8151/IL0373-совместимым (V1). Различить можно по работе дефолтного класса: SSD1680-классы при UC8151-панели виснут на BUSY (и наоборот). Поэтому дефолт SSD1680 + закомментированные альтернативы в конфиге — правильная стратегия; логика выбора одного define уже поддержана GxEPDDisplay без правок общих файлов.

## 5. Подсветка/LED
По решению пользователя LED — оповещение, не подсветка. Механизмы `DISP_BACKLIGHT`/`BACKLIGHT_BTN` в `GxEPDDisplay.cpp` к LuFree не применяются (это флаги подсветки OLED/LCD-вариантов).
