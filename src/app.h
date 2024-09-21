#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <vector>
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

typedef std::function<void(String, String)> MQTTCallback;

class App
{
private:
    String name;
    String version;
    String manufacturer;
    String devicetype;
    int serverPort;
    PubSubClient *pubsubclient;
    String mqttBrokerHost;
    String mqttBrokerUsername;
    String mqttBrokerPassword;
    int mqttBrokerPort;
    std::vector<MQTTCallback> mqttCallbacks;
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
    bool mqttinit;

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

    String computeTechnicalName();

    void setVersion(String version);

    void setManufacturer(String manufacturer);

    void setServerPort(int serverPort);

    void setMQTTBrokerHost(String mqttBrokerHost);

    void setMQTTBrokerUsername(String mqttBrokerUsername);

    void setMQTTBrokerPassword(String mqttBrokerPassword);

    void setMQTTBrokerPort(int mqttBrokerPort);

    void announceMDNS();

    void announceSSDP();

    const char *getSSDPSchema();

    void MQTT_init();

    void MQTT_reconnect();

    String MQTT_announce_button(String buttonId, String title, String icon, const std::function<void()> &clickHandler);

    String MQTT_announce_number(String numberId, String title, String icon, String mode, float min, float max, const std::function<void(String)> &changeHandler);

    String MQTT_announce_sensor(String notifyId, String title, String icon);

    String MQTT_announce_tagscanner(String notifyId);

    void MQTT_publish(String topic, String payload);

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
