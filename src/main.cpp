#include "boomboxmode.h"
#include "leds.h"
#include "logging.h"
#include "rfidplayermode.h"
#include "sensors.h"

#include <esp_arduino_version.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>

Leds *leds = NULL;
Sensors *sensors = NULL;
Mode *mode = NULL;

void setup() {
	Serial.begin(115200);

	INFO("Setup started");
	INFO("Running on Arduino : %d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
	INFO("Running on ESP IDF : %s", esp_get_idf_version());
	INFO("Max  HEAP  is %d", ESP.getHeapSize());
	INFO("Free HEAP  is %d", ESP.getFreeHeap());
	INFO("Max  PSRAM is %d", ESP.getPsramSize());
	INFO("Free PSRAM is %d", ESP.getFreePsram());

	INFO("Starting boot sequence");
	leds = new Leds();
	leds->begin();
	leds->setBootProgress(0);
	leds->setState(BOOT);

	sensors = new Sensors(leds);
	if (!sensors->isPreviousPressed()) {
		INFO("Booting into RFID player mode");
		mode = new RfidPlayerMode(leds, sensors);
	} else {
		INFO("Booting into Bluetooth Boombox mode");
		mode = new BoomboxMode(leds, sensors);
	}

	mode->setup();

	INFO("Setup finished.");
	INFO("Max  HEAP  is %d", ESP.getHeapSize());
	INFO("Free HEAP  is %d", ESP.getFreeHeap());
	INFO("Max  PSRAM is %d", ESP.getPsramSize());
	INFO("Free PSRAM is %d", ESP.getFreePsram());
}

void loop() {

	mode->loop();

	esp_task_wdt_reset();
}
