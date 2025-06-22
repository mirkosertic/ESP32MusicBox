#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <functional>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <mutex>

#include "tagscanner.h"
#include "commands.h"
#include "mediaplayer.h"
#include "mediaplayersource.h"
#include "voiceassistant.h"
#include "settings.h"

#include <BluetoothA2DPSink.h>

typedef std::function<void(bool active, float volume, const char *currentsong, int playprogressinpercent)> ChangeNotifierCallback;

class App
{
private:
    String name;
    String version;
    String manufacturer;
    String devicetype;
    int serverPort;
    long stateversion;
    char *currentpath;

    bool tagPresent;
    bool knownTag;
    TagData tagData;
    String tagName;
    uint8_t tagUid[8];
    uint8_t tagUidlength;

    float volume;

    TagScanner *tagscanner;

    bool wificonnected;

    MediaPlayerSource *source;
    MediaPlayer *player;
    VoiceAssistant *assistant;
    Settings *settings;

    VolumeSupport *volumeSupport;

    std::mutex loopmutex;

    ChangeNotifierCallback changecallback;

    WiFiUDP *udp;

    bool btspeakerconnected;

    bool actasbluetoothspeaker;
    bool btpause;
    BluetoothA2DPSink *bluetoothsink;

public:
    App(TagScanner *tagscanner, MediaPlayerSource *source, MediaPlayer *player, Settings *settings, VolumeSupport *volumeSupport);

    ~App();

    void begin(ChangeNotifierCallback callback);

    void setWifiConnected();

    bool isWifiEnabled();

    bool isWifiConnected();

    void noTagPresent();

    void setTagData(bool knownTag, String tagName, uint8_t *uid, uint8_t uidLength, TagData tagData);

    bool getTagPresent();

    String getTagName();

    String getTagInfoText();

    bool getIsKnownTag();

    char *getCurrentPath();

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

    String getConfigurationURL();

    void announceMDNS();

    void announceSSDP();

    void ssdpNotify();

    String getSSDPDescription();

    void writeCommandToTag(CommandData command);

    void clearTag();

    void loop();

    float getVolume();

    void publishState();

    bool isActive();

    const char *currentTitle();

    int playProgressInPercent();

    void setVolume(float volume);

    bool volumeUp();

    bool volumeDown();

    void toggleActiveState();

    void previous();

    void next();

    void play(String path, int index);

    void setBluetoothSpeakerConnected(bool value = true);

    bool isBluetoothSpeakerConnected();

    void actAsBluetoothSpeaker(BluetoothA2DPSink *bluetoothsink);

    bool isActAsBluetoothSpeaker();
};

#endif
