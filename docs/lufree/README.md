# LuFree — прошивка MeshCore для «лучика»

**LuFree** — вариант companion-прошивки [MeshCore](../../README.md) для устройства «лучик»:
клон Heltec **HT-RA62** (**nRF52840 + Semtech SX1262**, «Faketec») + экран **WeAct Studio 2.9" e-paper 128x296** (SPI) + **5-позиционный стик** + отдельный **LED-диод оповещения** (не подсветка).

Прошивка основана на стандартном companion MeshCore (UI `ui-new`, BLE/USB), автосборка CI в этом репозитории настроена только на LuFree.

## Загрузка и прошивка

Готовая сборка публикуется в разделе [Releases](https://github.com/Trahomich/MeshCore/releases) — rolling-прелиз **`lufree-latest`** обновляется при каждом push в `main`. Два файла:

- **`lufree_companion_radio_ble.zip`** — DFU-пакет (внутри `manifest.json` + `firmware.bin/.dat`, `sd_req 0x0123` = S140 7.3.0). Прошивается штатными инструментами:
  - **nRF Connect for Desktop → Programmer**: Add file → выбрать zip → Write;
  - **Web Device View / adafruit web flasher**: открыть страницу, подключить устройство по USB, выбрать zip.
- **`lufree_companion_radio_ble.uf2`** — для DFU-бутлоадера: дважды быстро нажать RESET (или RESET+BOOT), появится диск `LUFBTBOOT`, скопировать файл туда (drag&drop), устройство само перезагрузится.

Важно: это приложение без SoftDevice — DFU-бутлоадер и S140 должны уже быть на плате (штатно для XIAO nRF52840 / клонов HT-RA62). OTA-прошивка из приложения MeshCore использует свой механизм обновления.

## Сборка

```bash
pio run -e lufree_companion_radio_ble     # BLE-компаньон (основная)
pio run -e lufree_companion_radio_usb     # USB-компаньон
```

Артефакт релиза (uf2 + zip), как в CI:

```bash
export FIRMWARE_VERSION=v0.0.1
bash build.sh build-companion-firmwares   # собирает только LuFree, результат в out/
```

## Настройка под плату

**Все номера пинов — заготовки: пользователь не подтверждал раскладку.** Перед прошивкой первого устройства сверьте их со схемой/платой:

| Что | Где менять | Значение-заготовка |
|---|---|---|
| Радио SX1262 (NSS/DIO1/RST/BUSY, SPI0) | `variants/lufree/variant.h`, секция «SPI0 — радио» | P0.07/P0.08/P0.09/P0.10, SCK=P0.04, MOSI=P0.05, MISO=P0.06 |
| Экран (CS/DC/RST/BUSY, SPI1) | `variants/lufree/variant.h`, секция «SPI1 — e-paper» | CS=13, DC=14, RST=15, BUSY=16, SCK=11, MOSI=12 |
| Стик (UP/DOWN/LEFT/RIGHT/OK) | `variants/lufree/variant.h`, секция «5-позиционный стик» | 17/18/19/20/21 (LOW + внутренняя подтяжка) |
| LED оповещения | `variants/lufree/variant.h`, `PIN_LED_NOTIFY` | 22, активный уровень `LED_STATE_ON = 1` |
| Батарея | `variants/lufree/variant.h`, `PIN_VBAT_READ`/`ADC_MULTIPLIER` | P0.31, множитель 4.9 |

Пины помечены комментариями `PLACEHOLDER - сверить с платой`.

### Контроллер экрана

Модель панели выбирается одним define в `variants/lufree/platformio.ini` (`EINK_DISPLAY_MODEL`):

- по умолчанию `GxEPD2_290_GDEY029T94` (контроллер **SSD1680** — самый частый у WeAct 2.9" 128x296);
- рядом закомментированы альтернативы для **UC8151D** (`GxEPD2_290_T5D`, `GxEPD2_290_M06`), **IL0373** (`GxEPD2_290_T5`), Waveshare V2 (`GxEPD2_290_T94_V2`) и др.

Если экран не оживает — переключите модель на соседнюю (как определить контроллер по поведению линии BUSY, см. `research-epaper.md`). Если картинка лежит вверх ногами — поменяйте `DISPLAY_ROTATION=1` на `3`.

### Радио

Основная ветка — SX1262 (`CustomSX1262`). Если на конкретной плате окажется LR1110 — переключить на `CustomLR1110/CustomLR1110Wrapper` по образцу `variants/wio_wm1110` (см. `research-input-board.md`).

## Управление (5-позиционный стик)

| Действие | Результат |
|---|---|
| UP / DOWN | навигация по спискам |
| OK (нажатие) | выбор (долгое нажатие — доп. действие, как у user-кнопки) |
| LEFT | назад |
| RIGHT | вперёд / выбор |
| тройной клик LEFT | контекстное меню (KEY_SELECT) — отдельной кнопки назад на устройстве нет |

LED мигает при непрочитанных сообщениях (штатный механизм `PIN_STATUS_LED`).

## CI

Автосборка (теги `companion-*`, `workflow_dispatch`) собирает только `lufree_companion_radio_ble`; автосборка repeater/room-server отключена (закомментированные триггеры в их workflow); `pr-build-check` проверяет только LuFree и при push в `main` публикует `uf2` + DFU-zip в rolling-релиз `lufree-latest` (см. «Загрузка и прошивка»). Возврат к полной автосборке — раскомментировать `build_all_firmwares_by_suffix` в `build.sh` и push-блоки в workflow-файлах.

## Подробнее

- `research-build.md` — как устроены варианты MeshCore и CI;
- `research-epaper.md` — e-paper, выбор контроллера, масштабы;
- `research-input-board.md` — ввод (стик/кнопки), nRF52840, LED.

Известные ограничения: пины не сверены с реальной платой (нет устройства в наличии), делитель батареи и активный уровень LED — допущения; на железе не тестировалось.
