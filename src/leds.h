#ifndef LEDS_H
#define LEDS_H

#include <FastLED.h>

enum LEDState {
	BOOT,
	PLAYER_STATUS,
	CARD_ERROR,
	CARD_DETECTED,
	VOLUME_CHANGE,
	BTCONNECTING,
	BTCONNECTED,
	RGBTEST
};

class Leds {
private:
	LEDState state;
	CRGB leds[NUM_LEDS];
	CRGB templeds[NUM_LEDS];
	long lastStateTime;
	long lastLoopTime;
	int framecounter;

	bool btspeakerconnected;
	int testr;
	int testg;
	int testb;

	void renderPlayerStatusIdle(bool wifiEnabled, bool wifiConnected);
	void renderPlayerStatusPlaying(int progressPercent);
	void renderCardError();
	void renderCardDetected();
	void renderVolumeChange(int volumePercent);
	void renderBTConnected();
	void renderBTConnecting();
	void renderRGBTest();

	void show();

public:
	Leds();

	void begin();

	void setState(LEDState state);

	void setBootProgress(int percent);

	void loop(bool wifiEnabled, bool wifiConnected, bool playbackActive, int volumePercent, int progressPercent);

	void setBluetoothSpeakerConnected(bool value = true);

	void end();

	void rgbtest(int r, int g, int b);
};

#endif
