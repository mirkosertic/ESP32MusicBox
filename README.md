# ESP32 MusicBox

[![PlatformIO CI](https://github.com/mirkosertic/ESP32MusicBox/actions/workflows/build.yml/badge.svg)](https://github.com/mirkosertic/ESP32MusicBox/actions/workflows/build.yml)

This project provides an ESP32 based music box with the following features:

* Simple 3-button control
* MP3 playback
* Integration with I2C RFID tag scanner
* Support for WiFi Grids with auto-reconnect to nearest access point
* MQTT status reporting / remote control
* Home Assistant integration with device autodiscovery
* Web-based user interface
* Integrated into your local network with SSDP / mDNS protocol

## Supported hardware

* Ai-Thinker ESP32-A1S-Audiokit v2.2 (Tested with release A402)
* PN532 RFID Reader (i2c-Mode)
* Ai-Thinker Board uses SD-Card in MMC 1Bit Mode. Buttons on Board must be 1=on(IO13 = KEY2), 2=off (), 3=on(IO15 = CMD), 4=off, 5=off.
* Neopixel / WS2812 bases LEDs for status visualization

## Schematics

The KiCad 8.0 project is located in the kicad folder.

Here is the wiring schematic drawing:

![schematics](doc/schematics.svg)

## Manual

Status visualization with LEDs:

- Boot-Sequence - LEDs enabled step by step to visualize boot progress
- Leds cycling in brightness - orange in case Wifi is not connected but enabled, green in case if playback paused or no playback yet
- Leds green from 0% to 100% filling the circle - Playback progress while playing something
- Leds blinking red - Unknown RFID card detected
- Leds blinking green - RFID card programmed / Known card detected
- Leds green to red - While changing volume
