# Цели сборки «Лучика»: соответствие вариантам MeshCore и распиновки

Исследование задачи #3. Источники: репозиторий (HEAD 5cbcb7a), docs/lufree/research-*.md
(прошлые итерации), внешние репозитории MeshCore-вариантов для модуля HT-RA62:
- https://github.com/AlexandrTyshchenko/Faketec-RA62-meshcore → variants/faketec_ra62
- https://github.com/pom3ranc86/supermini_nrf_ht_ra62 → variants/supermini_nrf_ht_ra62

## Вывод (главное)

Все три цели строятся на **одной платформе ProMicro nRF52840** (board `promicro_nrf52840`,
пины `variants/promicro/PromicroBoard.h`) и отличаются радио-модулем/экраном:

| Цель (env по CONTRACTS.md) | База | Радио | Экран | Отличия от promicro |
|---|---|---|---|---|
| `luchik_promicro` | `variants/promicro` (секция `[Promicro]`) | fakeTec SX1262, пины PromicroBoard.h | нет (NullDisplay) | только ADVERT_NAME="Luchik", убрать GPS/sensors-флаги не обязательно |
| `luchik_weact290` | `variants/lufree` (env `lufree_companion_radio_ble/_usb`) | тот же fakeTec SX1262 (не тронут в lufree) | WeAct 2.9" SSD1680, пины — рантайм-автоподбор (`LUFREE_EINK`, EPaperScan.cpp) + 5-стик (`LUFREE_STICK`, StickScan.cpp) | только ADVERT_NAME="Luchik" |
| `luchik_ra62` | `variants/promicro` + радио-флаги RA62 (ниже) | **модуль HT-RA62** (SX1262, TCXO 1.8 В, DIO2 — RF-свитч) | нет (NullDisplay) | флаги RA62 + `SX126X_REGISTER_PATCH=1` |

## Пины (стандартные, единые для всех трёх целей)

`variants/promicro/PromicroBoard.h` (подтверждены пользователем в прошлой итерации — BLE/LoRa работали):

```
P_LORA_NSS    13   // P1.13
P_LORA_DIO_1  11   // P0.10
P_LORA_RESET  10   // P0.09
P_LORA_BUSY   16   // P0.29
P_LORA_MISO   15   // P0.02
P_LORA_SCLK   12   // P1.11
P_LORA_MOSI   14   // P1.15
```
Прочее из `[Promicro]`: SCL=7, SDA=8, USER_BTN=6, GPS RX/TX/EN=3/4/5.

## Радио-флаги для `luchik_ra62` (стандартная обвязка HT-RA62, сверено с двумя внешними вариантами)

```
-D USE_SX1262
-D RADIO_CLASS=CustomSX1262
-D WRAPPER_CLASS=CustomSX1262Wrapper
-D LORA_TX_POWER=22
-D SX126X_CURRENT_LIMIT=140
-D SX126X_RX_BOOSTED_GAIN=1
-D SX126X_DIO2_AS_RF_SWITCH=1     ; у RA62 DIO2 управляет RF-свитчем
-D SX126X_DIO3_TCXO_VOLTAGE=1.8   ; на модуле TCXO 1.8 В
-D SX126X_REGISTER_PATCH=1        ; патч регистров для RA62 (из faketec_ra62)
```
Частота/параметры LoRa — базовые по контракту (LORA_FREQ=869.618 и т.п. из `arduino_base`), как у promicro.

Прецеденты: faketec_ra62 extends `Promicro`, «Board sources are shared with variants/promicro»,
те же пины + `SX126X_REGISTER_PATCH=1`; supermini_nrf_ht_ra62 — board `promicro_nrf52840`,
`PromicroBoard.h`, + DIO2_AS_RF_SWITCH/DIO3_TCXO. GAT562* — другие платы (RAK4630-вариант
variant.h), для RA-62 НЕ подходят.

## Что создавать нового (задачам #4/#5)

`variants/luchik/platformio.ini` (по CONTRACTS.md раздел 3), свои файлы платы НЕ нужны —
переиспользуем `variants/promicro/*` (как это делает lufree через build_src_filter) и
`variants/lufree/{EPaperScan,StickScan}.cpp` для weact290. Для ra62 — только флаги выше.
Env-имена: `luchik_promicro_ble/_usb`, `luchik_weact290_ble/_usb`, `luchik_ra62_ble/_usb`;
для weact290 взять за образец `lufree_companion_radio_ble/_usb` целиком (ldscript
`boards/nrf52840_s140_v6_extrafs.ld`, maximum_size 712704, lib_deps с GxEPD2).

## Допущения

- «ra-62» = радио-модуль Heltec HT-RA62 (nRF52840-совместимая обвязка SX1262 на пинах promicro),
  а не платы GAT562 (те основаны на RAK4630-распиновке).
- Радиопараметры (869.618 МГц, BW 62.5, SF 8) оставляем базовые — как в `arduino_base`.
