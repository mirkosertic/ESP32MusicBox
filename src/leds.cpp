#include <Arduino.h>

#include "leds.h"

#include "logging.h"
#include "pins.h"

DEFINE_GRADIENT_PALETTE(volume_heatmap) {
	224, 0, 255, 0,
	224, 255, 0, 0};

Leds::Leds() {
	this->btspeakerconnected = false;
	for (int i = 0; i < NUM_LEDS; i++) {
		this->leds[i] = CRGB::Black;
	}

	FastLED.addLeds<NEOPIXEL, GPIO_NEOPIXEL_DATA>(leds, NUM_LEDS);

	this->lastLoopTime = 0;
	this->framecounter = 0;
}

void Leds::begin() {
	FastLED.show();
}

void Leds::setState(LEDState state) {
	INFO("Changing STATE");
	this->state = state;
	this->lastStateTime = millis();
}

void Leds::setBootProgress(int percent) {
	this->state = BOOT;

	for (int i = 0; i < NUM_LEDS; i++) {
		this->leds[i] = CRGB::Black;
	}

	int progress = (int) (NUM_LEDS / 100.0 * min(percent, 100));
	for (int i = 0; i < progress; i++) {
		this->leds[i] = CRGB::White;
	}
	FastLED.show();
}

void Leds::renderPlayerStatusIdle(bool wifiEnabled, bool wifiConnected) {
	CRGB color;
	if (wifiConnected || !wifiEnabled) {
		color = this->btspeakerconnected ? CRGB::Blue : CRGB::Green;
	} else {
		color = CRGB::Orange;
	}
	float minv = 40;
	float maxv = 192;
	int steps = 30;
	int framepos = this->framecounter % (steps * 2);
	int v = 0;
	if (framepos < steps) {
		v = (int) (minv + ((maxv - minv) / steps * framepos));
	} else {
		v = (int) (maxv - ((maxv - minv) / steps * (framepos - steps)));
	}
	for (int i = 0; i < NUM_LEDS; i++) {
		this->leds[i] = color % v;
	}
	FastLED.show();
}

void Leds::renderPlayerStatusPlaying(int progressPercent) {
	for (int i = 0; i < NUM_LEDS; i++) {
		this->leds[i] = CRGB::Black;
	}

	int maxbrightness = 80;

	int total = NUM_LEDS * maxbrightness;
	int progress = (int) (total / 100.0 * progressPercent);
	int index = 0;
	while (progress > 0) {
		int v = min(maxbrightness, progress);
		CHSV targetHSV = rgb2hsv_approximate(this->btspeakerconnected ? CRGB::Blue : CRGB::Green);
		targetHSV.v = v;
		this->leds[index++] = targetHSV;
		progress -= v;
	}
	FastLED.show();
}

void Leds::renderCardError() {
	long now = millis();
	if (now - this->lastStateTime > 1000) {
		// After one second inactivity / animation we jump back to the regular status indication
		this->state = PLAYER_STATUS;
		INFO("Switching state to PLAYER_STATUS");
	} else {
		// Red blinking
		int framepos = this->framecounter % 10;
		for (int i = 0; i < NUM_LEDS; i++) {
			this->leds[i] = CRGB::Black;
		}
		if (framepos > 5) {
			// All Status Red
			for (int i = 0; i < NUM_LEDS; i++) {
				this->leds[i] = CRGB::Red;
			}
		}
		FastLED.show();
	}
}

void Leds::renderCardDetected() {
	long now = millis();
	if (now - this->lastStateTime > 1000) {
		// After one second inactivity / animation we jump back to the regular status indication
		this->state = PLAYER_STATUS;
		INFO("Switching state to PLAYER_STATUS");
	} else {
		// Green blinking
		int framepos = this->framecounter % 10;
		for (int i = 0; i < NUM_LEDS; i++) {
			this->leds[i] = CRGB::Black;
		}
		if (framepos > 5) {
			// All Status Red
			for (int i = 0; i < NUM_LEDS; i++) {
				this->leds[i] = CRGB::Green;
			}
		}
		FastLED.show();
	}
}

void Leds::renderBTConnecting() {
	long now = millis();
	if (now - this->lastStateTime > 5000) {
		// After one second inactivity / animation we jump back to the regular status indication
		this->state = PLAYER_STATUS;
		INFO("Switching state to PLAYER_STATUS");
	} else {
		// Yellow blinking
		int framepos = this->framecounter % 10;
		for (int i = 0; i < NUM_LEDS; i++) {
			this->leds[i] = CRGB::Black;
		}
		if (framepos > 5) {
			for (int i = 0; i < NUM_LEDS; i++) {
				this->leds[i] = CRGB::Yellow % 192;
			}
		}
		FastLED.show();
	}
}

void Leds::renderBTConnected() {
	long now = millis();
	if (now - this->lastStateTime > 2000) {
		// After one second inactivity / animation we jump back to the regular status indication
		this->state = PLAYER_STATUS;
		INFO("Switching state to PLAYER_STATUS");
	} else {
		// Blue blinking
		int framepos = this->framecounter % 10;
		for (int i = 0; i < NUM_LEDS; i++) {
			this->leds[i] = CRGB::Black;
		}
		if (framepos > 5) {
			for (int i = 0; i < NUM_LEDS; i++) {
				this->leds[i] = CRGB::Blue % 192;
			}
		}
		FastLED.show();
	}
}

void Leds::renderVolumeChange(int volumePercent) {
	long now = millis();
	if (now - this->lastStateTime > 1000) {
		// After one second inactivity / animation we jump back to the regular status indication
		this->state = PLAYER_STATUS;
		INFO("Switching state to PLAYER_STATUS");
	} else {
		static int lastVolumePercent = volumePercent;
		if (lastVolumePercent != volumePercent) {
			INFO("Volume is %d", volumePercent);
			lastVolumePercent = volumePercent;

			for (int i = 0; i < NUM_LEDS; i++) {
				this->leds[i] = CRGB::Black;
			}

			int maxbrightness = 80;

			CRGBPalette16 myPalette = volume_heatmap;

			int total = NUM_LEDS * maxbrightness;
			int progress = (int) (total / 100.0 * volumePercent);
			int index = 0;
			while (progress > 0) {
				int v = min(maxbrightness, progress);
				CRGB color = ColorFromPalette(myPalette, (int) (255.0 * index / NUM_LEDS), v);
				// CHSV targetHSV = rgb2hsv_approximate(CRGB::Orange);
				// targetHSV.v = v;
				this->leds[index++] = color;
				progress -= v;
			}
			FastLED.show();
		}
	}
}

void Leds::renderRGBTest() {
	for (int i = 0; i < NUM_LEDS; i++) {
		this->leds[i].r = testr;
		this->leds[i].g = testg;
		this->leds[i].b = testb;
	}
	FastLED.show();
}

void Leds::loop(bool wifiEnabled, bool wifiConnected, bool playbackActive, int volumePercent, int progressPercent) {
	long now = millis();
	if (this->lastLoopTime + 40 < now) {
		this->lastLoopTime = now;
		this->framecounter++;
		if (this->framecounter < 0) {
			this->framecounter = 0;
		}

		if (this->state == RGBTEST) {
			this->renderRGBTest();
		} else if (this->state == CARD_ERROR) {
			this->renderCardError();
		} else if (this->state == CARD_DETECTED) {
			this->renderCardDetected();
		} else if (this->state == BTCONNECTING) {
			this->renderBTConnecting();
		} else if (this->state == BTCONNECTED) {
			this->renderBTConnected();
		} else if (this->state == VOLUME_CHANGE) {
			this->renderVolumeChange(volumePercent);
		} else if (this->state == PLAYER_STATUS) {
			if (playbackActive) {
				this->renderPlayerStatusPlaying(progressPercent);
			} else {
				this->renderPlayerStatusIdle(wifiEnabled, wifiConnected);
			}
		}
	}
}

void Leds::setBluetoothSpeakerConnected(bool value) {
	this->btspeakerconnected = value;
}

void Leds::end() {
	FastLED.clear(true);
}

void Leds::rgbtest(int r, int g, int b) {
	testr = r;
	testg = g;
	testb = b;
	state = RGBTEST;
}
