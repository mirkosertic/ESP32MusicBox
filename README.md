# ESP32 MusicBox

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

## Manual

TBD
