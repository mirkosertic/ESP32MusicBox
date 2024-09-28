#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <WiFi.h>
#include <vector>

#include "settings.h"
#include "app.h"

typedef std::function<void(String, String)> MQTTCallback;

class MQTT
{
private:
    App *app;
    PubSubClient *pubsubclient;
    String clientid;
    String host;
    int port;
    String username;
    String password;
    std::vector<MQTTCallback> mqttCallbacks;

    String tagtopic;
    String tagscannertopic;
    String currentsongtopic;
    String playbackstatetopic;
    String volumestatetopic;

    String announceButton(String buttonId, String title, String icon, const std::function<void()> &clickHandler);

    String announceNumber(String numberId, String title, String icon, String mode, float min, float max, const std::function<void(String)> &changeHandler);

    String announceSensor(String notifyId, String title, String icon);

    String announceTagscanner(String notifyId);

    void performAutoDiscovery();

    void publish(String topic, String payload);

public:
    MQTT(WiFiClient &wifiClient, App *app);
    ~MQTT();

    void begin(String host, int port, String username, String password);

    void loop();

    void publishCurrentSong(String value);

    void publishPlaybackState(String value);

    void publishVolume(int value);

    void publishScannedTag(String value);

    void publishTagScannerInfo(String value);
};

#endif