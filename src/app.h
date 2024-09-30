#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <functional>
#include <WiFi.h>
#include <ESP32SSDP.h>
#include <PubSubClient.h>

#include "tagscanner.h"
#include "commands.h"
#include "mediaplayer.h"
#include "mediaplayersource.h"
#include "voiceassistant.h"
#include "settings.h"

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

    TagScanner *tagscanner;

    bool wificonnected;

    MediaPlayerSource *source;
    MediaPlayer *player;
    VoiceAssistant *assistant;
    Settings *settings;

public:
    App(WiFiClient &wifiClient, TagScanner *tagscanner, MediaPlayerSource *source, MediaPlayer *player, Settings *settings);

    ~App();

    void setWifiConnected();

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

    const char *getSSDPSchema();

    void writeCommandToTag(CommandData command);

    void clearTag();

    void loop();

    float getVolume();

    bool isActive();

    const char *currentTitle();

    void setVolume(float volume);

    void toggleActiveState();

    void previous();

    void next();

    void play(String path, int index);
};

#endif
