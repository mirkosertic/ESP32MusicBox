#include "boomboxmode.h"

#include "logging.h"
#include "pins.h"

#include <AudioTools.h>

BoomboxMode::BoomboxMode(Leds *leds, Sensors *sensors)
	: Mode(leds, sensors) {
	this->connected = false;
}

void BoomboxMode::setup() {
	Mode::setup();

	this->leds->setBootProgress(10);

	INFO("Bluetooth sink configuration. Free HEAP is %d", ESP.getFreeHeap());
	this->bluetoothsink = new BluetoothSink(this->i2sstream, [this](BluetoothSink *sink, esp_a2d_connection_state_t state) {
        switch (state)
        {
            case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                INFO("bluetooth() - DISCONNECTED");
                break;
            case ESP_A2D_CONNECTION_STATE_CONNECTING:
                INFO("bluetooth() - CONNECTING");
                this->leds->setState(BTCONNECTING);
                break;
            case ESP_A2D_CONNECTION_STATE_CONNECTED:
                INFO("bluetooth() - CONNECTED. Receiving data from Bluetooth source");
                this->leds->setBluetoothSpeakerConnected();
                this->leds->setState(BTCONNECTED);
                this->connected = true;
                break;
            case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
                INFO("bluetooth() - DISCONNECTING");
                break;
        }; });

	bluetoothsink->start(this->settings->computeTechnicalName());

	INFO("Bluetooth sink initialized. Free HEAP is %d", ESP.getFreeHeap());

	this->leds->setBootProgress(90);

	this->sensors->begin(this->bluetoothsink);

	// Boot complete
	this->leds->setBootProgress(100);

	this->leds->setState(PLAYER_STATUS);
	this->leds->setBluetoothSpeakerConnected();
}

ModeStatus BoomboxMode::loop() {
	Mode::loop();

	this->leds->loop(false, false, false, 100, 100);

	int pincode = this->bluetoothsink->pinCode();
	if (pincode != 0 && !this->connected) {
		DEBUG("Got PIN code %d for confirmation", pincode);
		if (this->sensors->isNextPressed()) {
			this->bluetoothsink->confirmPinCode();
		} else {
			static bool switchstate = false;
			if (!switchstate) {
				this->leds->setState(BTCONNECTING);
				switchstate = true;
			}
		}
	}

	if (this->connected) {
		return MODE_NOT_IDLE;
	}
	return MODE_IDLE;
}

void BoomboxMode::shutdown() {
	INFO("Shutting down everything")
	this->leds->end();
	esp_deep_sleep_start();
}
