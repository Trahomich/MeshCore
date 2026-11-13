# Luchik — альтернативная прошивка для «Лучика»

Форк [MeshCore](https://github.com/lixea-dev/MeshCore) (companion-прошивка) для
устройства «Лучик» на платформе **ProMicro nRF52840**. Позволяет пользоваться
LoRa-мессенджером MeshCore через приложение на телефоне (BLE) или USB-терминал.

Исходный апстрим: https://github.com/Trahomich/MeshCore

## Цели сборки

Шесть готовых образов — три аппаратные цели × два интерфейса (`_ble` — Bluetooth
для мобильного приложения MeshCore, `_usb` — USB CDC-терминал):

| Цель | Что это | Особенности |
|---|---|---|
| `luchik_promicro` | базовый вариант: ProMicro nRF52840 + fakeTec SX1262 | экрана нет; пины `variants/promicro/PromicroBoard.h` (проверены пользователем: BLE/LoRa работают) |
| `luchik_weact290` | + e-paper WeAct 2.9" (SSD1680, 128×296) и 5-позиционный стик | **распиновка экрана неизвестна** — пины подбирает сама прошивка при первом старте (`variants/lufree/EPaperScan.cpp`, секунды, максимум 10 минут; результат в `/epaper.cfg`); стик калибруется интерактивно (`StickScan.cpp`, `/stick.cfg`) |
| `luchik_ra62` | радио-модуль Heltec HT-RA62 (SX1262, TCXO 1.8 В, DIO2 — RF-свитч) | экрана нет; пины promicro + стандартная обвязка модуля (см. `docs/luchik-targets.md`) |

Радиопараметры — базовые MeshCore (869.618 МГц, SF8, BW 62.5), имя устройства
`Luchik`.

## Где брать прошивки

Автосборка: GitHub Actions (`.github/workflows/build-luchik.yml`) по каждому
push в `main`. Готовые файлы — в rolling-релизе **[luchik-latest](../../releases/tag/luchik-latest)**:
`luchik_<цель>_<ble|usb>.uf2` и `.zip`.

## Как прошивать

Оба способа используют встроенный DFU-бутлоадер nRF52840 (ничего дополнительно
ставить не нужно):

- **UF2 (проще всего):** коротко нажмите RESET дважды — появится диск
  `LUCHIKBOOT` (или `NRF52BOOT`); скопируйте туда файл `.uf2`. После копирования
  плата сама перезагрузится с новой прошивкой.
- **ZIP (DFU-пакет):** запустите плату в DFU-режиме (двойной RESET) и загрузите
  `.zip` через [nRF Connect Programmer](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop)
  или [Web Device View](https://developer.nordicsemi.com/.asset-files/website/20.10/nrf-device-viewer-wasm/).

## Ограничения и допущения

- Распиновки «Лучика» нет: используются стандартные пины варианта promicro
  (подтверждены пользователем в прошлой итерации) и рантайм-автоподбор экрана
  для WeAct 2.9". Если экран не найден, он отключается, BLE/LoRa продолжают
  работать.
- «Weact 290» = e-paper WeAct 2.9" (SSD1680); «ra-62» = модуль Heltec HT-RA62
  на пинах promicro (не платы GAT562 — у них распиновка RAK4630).
- GPS в luchik-вариантах отключён (на устройстве его нет, Serial1 освобождён
  для автоподбора экрана).
- Локальная сборка не поддерживается (требование владельца репозитория);
  собирайте через GitHub Actions.

## Сборка

Автосборка запускается автоматически при push в `main` или вручную
(workflow_dispatch, Actions → Build Luchik Firmware → Run workflow).

## Подробнее

- Соответствие целей и распиновка: [`docs/luchik-targets.md`](../luchik-targets.md)
- Базовый вариант и автоподбор экрана: `variants/lufree/` и `docs/lufree/`
- Про MeshCore: [`README.md`](../README.md), [`docs/`](../)
