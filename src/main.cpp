#include "boomboxmode.h"
#include "leds.h"
#include "logging.h"
#include "pins.h"
#include "rfidplayermode.h"
#include "sensors.h"

#include <driver/rtc_io.h>
#include <esp_arduino_version.h>
#include <esp_idf_version.h>
#include <esp_sleep.h>
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

	esp_sleep_wakeup_cause_t wakeupreason = esp_sleep_get_wakeup_cause();
	switch (wakeupreason) {
		case ESP_SLEEP_WAKEUP_EXT0: {
			INFO("Deepsleep Wakeup EXT0");
			break;
		}
		case ESP_SLEEP_WAKEUP_EXT1: {
			INFO("Deepsleep Wakeup EXT1");
			break;
		}
		case ESP_SLEEP_WAKEUP_TIMER: {
			INFO("Deepsleep Wakeup Timer");
			break;
		}
		case ESP_SLEEP_WAKEUP_TOUCHPAD: {
			INFO("Deepsleep Wakeup Touchpad");
			break;
		}
		default: {
			INFO("Wakeup not caused by known Deepsleep reason : %d", wakeupreason);
			break;
		}
	}

	INFO("Starting boot sequence");
	leds = new Leds();
	leds->begin();
	leds->setBootProgress(0);
	leds->setState(BOOT);

	sensors = new Sensors();
	if (!sensors->isPreviousPressed()) {
		INFO("Booting into RFID player mode");
		mode = new RfidPlayerMode(leds, sensors);
	} else {
		INFO("Booting into Bluetooth Boombox mode");
		mode = new BoomboxMode(leds, sensors);
	}

	mode->setup();

	// TODO: Wakeup on RFID IRQ??? Will only work after RFIDReaderMode is active...
	INFO("Configuring Deepsleep wakeup");
	if (esp_sleep_enable_ext0_wakeup(GPIO_STARTSTOP, 1)) {
		WARN("Failed to configure deepsleep!")
		while (true)
			;
	}
	rtc_gpio_pulldown_en(GPIO_STARTSTOP);
	rtc_gpio_pullup_dis(GPIO_STARTSTOP);

	INFO("Setup finished.");
	INFO("Max  HEAP  is %d", ESP.getHeapSize());
	INFO("Free HEAP  is %d", ESP.getFreeHeap());
	INFO("Max  PSRAM is %d", ESP.getPsramSize());
	INFO("Free PSRAM is %d", ESP.getFreePsram());
}

#define AUTO_SHUTDOWN_IN_MINUTES 10

long lastIdleTime = -1;

void loop() {

	if (mode->loop() == MODE_IDLE) {
		long now = millis();
		if (lastIdleTime == -1) {
			lastIdleTime = now;
		} else if (now - lastIdleTime > (AUTO_SHUTDOWN_IN_MINUTES * 60 * 1000)) {
			// Deep sleep
			INFO("Beeing idle for too long, going to deepsleep");
			leds->end();
			esp_deep_sleep_start();
		}
	} else {
		lastIdleTime = -1;
	}

	esp_task_wdt_reset();
}
