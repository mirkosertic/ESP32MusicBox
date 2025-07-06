#include "mode.h"

#include "globals.h"
#include "i2cdebug.h"
#include "logging.h"
#include "pins.h"

#include <SD.h>
#include <SPI.h>
#include <Wire.h>

Mode::Mode(Leds *leds, Sensors *sensors)
	: defaultAudioInfo(44100, 2, 16) {
	this->leds = leds;
	this->sensors = sensors;
}

void Mode::setup() {
	this->leds->setState(BOOT);
	this->leds->setBootProgress(0);

	this->i2cinit();
	this->i2sinit();
	this->sdinit();

	leds->setBootProgress(5);
}

void Mode::i2cinit() {
	INFO("I2C connection init");
	if (!Wire1.begin(GPIO_WIRE_SDA, GPIO_WIRE_SCL)) {
		WARN("I2C initialization failed!");
		while (true)
			;
	}

	// Some debug output
	I2CDebug debug(&Wire1);
	debug.printDevices();
}

void Mode::i2sinit() {
#if USE_AUDIO_LOGGING
	AudioLogger::instance().begin(Serial, AudioLogger::Warning);
#endif

	INFO("Initializing I2S sound system")
	this->i2sstream = new I2SStream();

	// setup output
	auto cfg = this->i2sstream->defaultConfig(TX_MODE);
	cfg.pin_bck = GPIO_I2S_BCK;
	cfg.pin_ws = GPIO_I2S_WS;
	cfg.pin_data = GPIO_I2S_DATA;
	cfg.i2s_format = I2S_STD_FORMAT; // default format
	cfg.copyFrom(this->defaultAudioInfo);

	if (!this->i2sstream->begin(cfg)) {
		WARN("Could not start I2S sound system!");
		while (true)
			;
	}
}

void Mode::sdinit() {
	// Setup SD-Card
	INFO("SD-Card init");
	SPI.begin(GPIO_SPI_SCK, GPIO_SPI_MISO, GPIO_SPI_MOSI, GPIO_SPI_SS);
	// SPIClass spi = SPIClass(VSPI);
	// spi.begin(GPIO_SPI_SCK, GPIO_SPI_MISO, GPIO_SPI_MOSI, GPIO_SPI_SS);

	// if (!SD.begin(GPIO_SPI_SS, spi))
	if (!SD.begin(GPIO_SPI_SS)) {
		WARN("Could not enable SD-Card over SPI!");
		while (true)
			;
	}

	this->settings = new Settings(&SD, CONFIGURATION_FILE);
	INFO("Retrieving system configuration file %s", CONFIGURATION_FILE);
	if (!settings->readFromConfig()) {
		WARN("Could not read configuration file from SDCard!");
		while (true)
			;
	}
}

ModeStatus Mode::loop() {
	this->sensors->loop();
	return MODE_NOT_IDLE;
}
