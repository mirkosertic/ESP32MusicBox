#include "mqtt.h"

#include <ArduinoJson.h>

#include "logging.h"

MQTT::MQTT(WiFiClient &wifiClient, App *app)
{
    this->app = app;
    this->pubsubclient = new PubSubClient(wifiClient);
    this->initialized = false;
}

MQTT::~MQTT()
{
    this->pubsubclient->disconnect();
    delete this->pubsubclient;
}

void MQTT::begin(String host, int port, String username, String password)
{
    this->clientid = app->computeTechnicalName();
    INFO("Connecting to %s:%d as client %s", host.c_str(), port, this->clientid.c_str());
    this->host = host;
    this->port = port;
    this->username = username;
    this->password = password;

    this->pubsubclient->setBufferSize(1024);
    this->pubsubclient->setServer(this->host.c_str(), this->port);
    this->pubsubclient->setCallback([this](char *topic, byte *payload, unsigned int length)
                                    {
                                        String to(topic);
                                        String messagePayload = String(payload, length);

                                        INFO("mqtt() - Got message on topic %s", topic);

                                        for (const auto& func : this->mqttCallbacks) {
                                            func(to, messagePayload);
                                        } });

    this->initialized = true;
}

void MQTT::loop()
{
    static long lastreconnectfailure = -1;

    if (this->initialized)
    {
        long now = millis();

        // Only try to reconnect every second
        if (!this->pubsubclient->connected() && now - lastreconnectfailure > 1000)
        {
            INFO("Needs to reconnect, trying to reconnect");
            bool result = this->pubsubclient->connect(this->clientid.c_str(), this->username.c_str(), this->password.c_str());
            if (!result)
            {
                WARN("Connection failed, rc=%d, try again...", this->pubsubclient->state());

                lastreconnectfailure = now;
            }
            else
            {
                INFO("Connected to broker!");
                this->performAutoDiscovery();

                // Subscribe to all state set topics
                String subscribeWildCard = this->clientid + "/+/set";
                INFO("Subscribing to topics %s", subscribeWildCard.c_str());
                this->pubsubclient->subscribe(subscribeWildCard.c_str());

                lastreconnectfailure = -1;
            }
        }
        else
        {
            static long lastwificheck = millis();

            this->pubsubclient->loop();

            if (now - lastwificheck > 30000)
            {
                INFO("I am still alive!");

                this->publishWiFiQuality(WiFi.RSSI());
                lastwificheck = now;
            }
        }
    }
}

void MQTT::performAutoDiscovery()
{
    INFO("Performing MQTT auto discovery");

    this->mqttCallbacks.clear();

    this->playbackstatetopic = this->announceSensor("playbackstate", "Playback", "mdi:information-outline");

    this->volumestatetopic = this->announceNumber("volume", "Volume", "mdi:volume-source", "slider", 0, 100, [this](String newvalue)
                                                  {
                                                    INFO("Got new volume as String %s", newvalue.c_str());
                                                    long volume = newvalue.toInt();
                                                    INFO("New volume as number is %ld", volume);
                                                    if (volume != 0)
                                                    {
                                                        this->app->setVolume(volume / 100.0); 
                                                    }
                                                    else
                                                    {
                                                        this->app->setVolume(0.0); 
                                                    } });

    this->announceButton("startstop", "Start/Stop", "mdi:restart", [this]()
                         { this->app->toggleActiveState(); });

    this->announceButton("next", "Next Title", "mdi:skip-next", [this]()
                         { this->app->next(); });

    this->announceButton("previous", "Previous Title", "mdi:skip-previous", [this]()
                         { this->app->previous(); });

    this->tagtopic = this->announceTagscanner("tags");

    this->tagscannertopic = this->announceSensor("tagscanner", "Detected Card", "mdi:tag-outline");

    this->currentsongtopic = this->announceSensor("currentsongstate", "Current Song", "mdi:information-outline");

    this->wifiqualitytopic = this->announceSensor("wifiquality", "WiFi Quality", "mdi:wifi", "SIGNAL_STRENGTH", "dBm");

    this->playprogressstatetopic = this->announceSensor("playprogress", "Play progress", "mdi:progress-clock", "", "%");

    this->voltagetopic = this->announceSensor("vcc", "Battery status", "mdi:car-battery", "VOLTAGE", "mV");
}

String MQTT::announceButton(String buttonId, String title, String icon, const std::function<void()> &clickHandler)
{
    String objectId = this->clientid + "_" + buttonId + "_btn";
    String discoveryTopic = "homeassistant/button/" + objectId + "/config";

    String topicPrefix = this->clientid + "/" + buttonId;
    String commandTopic = topicPrefix + "/set";
    String stateTopic = topicPrefix + "/state";

    JsonDocument discoverypayload;
    discoverypayload["object_id"] = objectId;
    discoverypayload["unique_id"] = objectId;
    discoverypayload["state_topic"] = stateTopic;
    discoverypayload["command_topic"] = commandTopic;
    discoverypayload["name"] = title;
    discoverypayload["icon"] = icon;

    JsonObject device = discoverypayload["device"].to<JsonObject>();
    device["name"] = this->app->getName();
    device["model"] = this->app->getDeviceType();
    device["manufacturer"] = this->app->getManufacturer();
    device["model_id"] = this->app->getVersion();
    device["sw_version"] = this->app->getSoftwareVersion();
    device["serial_number"] = this->app->computeSerialNumber();
    device["configuration_url"] = this->app->getConfigurationURL();

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->app->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->mqttCallbacks.push_back([commandTopic, clickHandler](String topic, String payload)
                                  {
            if (topic == commandTopic) {
                INFO("Got Message on %s -> %s", commandTopic.c_str(), payload.c_str());
                if (payload == "PRESS") {
                    INFO("Pressed!");
                    clickHandler();
                }
            } });

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return stateTopic;
}

String MQTT::announceNumber(String numberId, String title, String icon, String mode, float min, float max, const std::function<void(String)> &changeHandler)
{
    String objectId = this->clientid + "_" + numberId + "_nbr";
    String discoveryTopic = "homeassistant/number/" + objectId + "/config";

    String topicPrefix = this->clientid + "/" + numberId;
    String commandTopic = topicPrefix + "/set";
    String stateTopic = topicPrefix + "/state";

    JsonDocument discoverypayload;
    discoverypayload["object_id"] = objectId;
    discoverypayload["unique_id"] = objectId;
    discoverypayload["state_topic"] = stateTopic;
    discoverypayload["command_topic"] = commandTopic;
    discoverypayload["name"] = title;
    discoverypayload["icon"] = icon;
    discoverypayload["mode"] = mode;
    discoverypayload["min"] = min;
    discoverypayload["max"] = max;

    JsonObject device = discoverypayload["device"].to<JsonObject>();
    device["name"] = this->app->getName();
    device["model"] = this->app->getDeviceType();
    device["manufacturer"] = this->app->getManufacturer();
    device["model_id"] = this->app->getVersion();
    device["sw_version"] = this->app->getSoftwareVersion();
    device["serial_number"] = this->app->computeSerialNumber();
    device["configuration_url"] = this->app->getConfigurationURL();

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->app->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->mqttCallbacks.push_back([commandTopic, changeHandler](String topic, String payload)
                                  {
            if (topic == commandTopic) {
                INFO("Changed topic %s with payload %s", topic.c_str(), payload.c_str());
                changeHandler(payload);
            } });

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return stateTopic;
}

String MQTT::announceSensor(String notifyId, String title, String icon, String deviceclass, String displayUnit)
{
    String objectId = this->clientid + "_" + notifyId + "_sens";
    String discoveryTopic = "homeassistant/sensor/" + objectId + "/config";

    String topicPrefix = this->clientid + "/" + notifyId;
    String stateTopic = topicPrefix + "/state";

    JsonDocument discoverypayload;
    discoverypayload["object_id"] = objectId;
    discoverypayload["unique_id"] = objectId;
    discoverypayload["state_topic"] = stateTopic;
    discoverypayload["name"] = title;
    discoverypayload["icon"] = icon;

    if (deviceclass.length() > 0)
    {
        discoverypayload["device_class"] = deviceclass;
    }

    if (displayUnit.length() > 0)
    {
        discoverypayload["unit_of_measurement"] = displayUnit;
    }

    JsonObject device = discoverypayload["device"].to<JsonObject>();
    device["name"] = this->app->getName();
    device["model"] = this->app->getDeviceType();
    device["manufacturer"] = this->app->getManufacturer();
    device["model_id"] = this->app->getVersion();
    device["sw_version"] = this->app->getSoftwareVersion();
    device["serial_number"] = this->app->computeSerialNumber();
    device["configuration_url"] = this->app->getConfigurationURL();

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->app->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return stateTopic;
}

String MQTT::announceTagscanner(String notifyId)
{
    String objectId = this->clientid + "_" + notifyId + "_tagscan";
    String discoveryTopic = "homeassistant/tag/" + objectId + "/config";

    String topicPrefix = this->clientid + "/" + notifyId;
    String topic = topicPrefix + "/state";

    JsonDocument discoverypayload;
    discoverypayload["object_id"] = objectId;
    discoverypayload["unique_id"] = objectId;
    discoverypayload["topic"] = topic;
    discoverypayload["value_template"] = "{{value_json.UID}}";

    JsonObject device = discoverypayload["device"].to<JsonObject>();
    device["name"] = this->app->getName();
    device["model"] = this->app->getDeviceType();
    device["manufacturer"] = this->app->getManufacturer();
    device["model_id"] = this->app->getVersion();
    device["sw_version"] = this->app->getSoftwareVersion();
    device["serial_number"] = this->app->computeSerialNumber();
    device["configuration_url"] = this->app->getConfigurationURL();

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->app->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return topic;
}

void MQTT::publish(String topic, String payload)
{
    if (this->pubsubclient->connected())
    {
        this->pubsubclient->publish(topic.c_str(), payload.c_str());
    }
    else
    {
        WARN("Cannot publish to topic %s -> value %s. The client is not connected", topic.c_str(), payload.c_str());
    }
}

void MQTT::publishCurrentSong(String value)
{
    this->publish(this->currentsongtopic, value);
}

void MQTT::publishPlaybackState(String value)
{
    this->publish(this->playbackstatetopic, value);
}

void MQTT::publishVolume(int value)
{
    this->publish(this->volumestatetopic, String(value));
}

void MQTT::publishScannedTag(String value)
{
    this->publish(this->tagtopic, value);
}

void MQTT::publishTagScannerInfo(String value)
{
    this->publish(this->tagscannertopic, value);
}

void MQTT::publishWiFiQuality(int rssi)
{
    this->publish(this->wifiqualitytopic, String(rssi));
}

void MQTT::publishPlayProgress(int progressInPercent)
{
    this->publish(this->playprogressstatetopic, String(progressInPercent));
}

void MQTT::publishBatteryVoltage(int voltage)
{
    this->publish(this->voltagetopic, String(voltage));
}