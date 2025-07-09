#ifndef RFIDPLAYERMODE_H
#define RFIDPLAYERMODE_H

#include "WiFiClient.h"
#include "app.h"
#include "bluetoothsource.h"
#include "mediaplayer.h"
#include "mode.h"
#include "mqtt.h"
#include "sdmediaplayersource.h"
#include "tagscanner.h"
#include "voiceassistant.h"
#include "webserver.h"

#include <AudioTools/AudioCodecs/CodecMP3Helix.h>

class RfidPlayerMode : public Mode {
private:
	SDMediaPlayerSource *source;
	MP3DecoderHelix *decoder;
	MediaPlayer *player;
	TagScanner *tagscanner;
	App *app;
	WiFiClient *wifiClient;
	Webserver *webserver;
	MQTT *mqtt;
	VoiceAssistant *assistant;
	BufferRTOS<uint8_t> *buffer;
	QueueStream<uint8_t> *bluetoothout;
	BluetoothSource *bluetoothsource;
	QueueHandle_t commandsHandle;

	bool bluetoothSpeakerConnected;
	bool wifienabled;
	bool wifiinitialized;

	void wifiConnected();

public:
	RfidPlayerMode(Leds *leds, Sensors *sensors);

	virtual void setup() override;
	virtual ModeStatus loop() override;
	virtual void prepareDeepSleep() override;
};

#endif
