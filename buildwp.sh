#!/usr/bin/env bash

export FIRMWARE_VERSION=v1.14.2
export PLATFORMIO_BUILD_FLAGS="-DTRANSLIT"
sh build.sh build-firmware Heltec_Wireless_Paper_companion_radio_ble
