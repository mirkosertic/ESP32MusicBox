#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include "settings.h"

#include "bluetoothsource.h"
#include "commands.h"
#include "leds.h"
#include "mediaplayer.h"
#include "playstatemonitor.h"
#include "tagscanner.h"
#include "userfeedbackhandler.h"
#include "voiceassistant.h"

#include <PubSubClient.h>
#include <functional>
#include <mutex>

typedef std::function<void(bool active, float volume, const char *currentsong, int playprogressinpercent)> ChangeNotifierCallback;

class App : public UserfeedbackHandler {
private:
	String name;
	String version;
	String manufacturer;
	String devicetype;
	int serverPort;
	long stateversion;

	bool tagPresent;
	bool knownTag;
	TagData tagData;
	String tagName;
	uint8_t tagUid[8];
	uint8_t tagUidlength;

	TagScanner *tagscanner;

	Leds *leds;
	MediaPlayer *player;
	VoiceAssistant *assistant;
	Settings *settings;
	BluetoothSource *bluetoothsource;

	std::mutex loopmutex;

	ChangeNotifierCallback changecallback;

	Equalizer3Bands *equalizer;
	PlaystateMonitor *monitor;

public:
	App(Leds *leds, TagScanner *tagscanner, MediaPlayer *player, Settings *settings, BluetoothSource *bluetoothsource, Equalizer3Bands *equalizer, PlaystateMonitor *monitor);
	~App();

	void begin(ChangeNotifierCallback callback);

	bool isWifiEnabled();

	void noTagPresent();

	void setTagData(bool knownTag, String tagName, uint8_t *uid, uint8_t uidLength, TagData tagData);

	bool getTagPresent();

	String getTagName();

	String getTagInfoText();

	bool getIsKnownTag();

	void incrementStateVersion();

	long getStateVersion();

	String computeUUID();

	String computeSerialNumber();

	void setName(String name);

	String getName();

	void setDeviceType(String devicetype);

	String getDeviceType();

	String computeTechnicalName();

	void setVersion(String version);

	String getVersion();

	String getSoftwareVersion();

	void setManufacturer(String manufacturer);

	String getManufacturer();

	void setServerPort(int serverPort);

	void writeCommandToTag(CommandData command);

	void clearTag();

	void loop();

	float getVolume();

	void publishState();

	bool isActive();

	const char *currentTitle();

	int playProgressInPercent();

	void setVolume(float volume, bool publishstate = true);

	bool volumeUp() override;

	bool volumeDown() override;

	void toggleActiveState() override;

	void previous() override;

	void next() override;

	void resetPlaybackToStart() override;

	void play(String path, int index);

	void playURL(String url);

	void shutdown();

	void rgbtest(int r, int g, int b);

	void equalizerLow(float value);

	void equalizerMiddle(float value);

	void equalizerHigh(float value);
};

#endif
