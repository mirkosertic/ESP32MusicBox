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

* ESP32 (tested with ESP32 DevKit C)
* PN532 RFID Reader (i2c-Mode)
* SDCard Reader (SPI-Mode)
* Neopixel / WS2812 bases LEDs for status visualization

## Schematics

The KiCad 9.0 project is located in the kicad folder.

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
