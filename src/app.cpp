#include "app.h"

#include <ESPmDNS.h>
#include <ESP32SSDP.h>
#include <ArduinoJson.h>

#include <esp_system.h>

App::App(TagScanner *tagscanner, MediaPlayerSource *source, MediaPlayer *player)
{
    this->tagscanner = tagscanner;
    this->espclient = new WiFiClient();
    this->pubsubclient = new PubSubClient(*this->espclient);
    this->stateversion = 0;
    this->currentpath = new char[512];
    this->tagPresent = false;
    this->tagName = "";
    this->wificonnected = false;
    this->mqttinit = false;

    this->source = source;
    this->player = player;
}

App::~App()
{
    delete this->pubsubclient;
    ;
    delete this->espclient;
    ;
}

void App::setWifiConnected()
{
    this->wificonnected = true;
}

bool App::isWifiConnected()
{
    return this->wificonnected;
}

void App::noTagPresent()
{
    if (this->tagPresent)
    {
        this->tagPresent = false;
        this->knownTag = false;
        this->tagName = "";
        this->tagData = TagData();

        this->incrementStateVersion();
    }
}

void App::setTagData(bool knownTag, String tagName, uint8_t *uid, uint8_t uidLength, TagData tagData)
{
    if (this->tagName != tagName || !this->tagPresent)
    {
        this->tagPresent = true;
        this->knownTag = knownTag;
        this->tagName = tagName;
        this->tagData = tagData;

        this->incrementStateVersion();
    }
}

bool App::getTagPresent()
{
    return this->tagPresent;
}

String App::getTagName()
{
    return this->tagName;
}

bool App::getIsKnownTag()
{
    return this->knownTag;
}

String App::getTagInfoText()
{
    if (this->knownTag)
    {
        CommandData command;
        memcpy(&command, &this->tagData.data[0], 44);
        if (command.version == COMMAND_VERSION)
        {
            if (command.command == COMMAND_PLAY_DIRECTORY)
            {
                String info = "Play directory ";
                info += String((char *)&command.path[0]);
                info += String(" with Volume ");
                info += command.volume;

                return info;
            }
            return String("Unknown command : ") + command.command;
        }
        return String("Unknown version : ") + command.version;
    }
    else
    {
        return "";
    }
}

char *App::getCurrentPath()
{
    return this->currentpath;
}

void App::incrementStateVersion()
{
    this->stateversion = this->stateversion + 1;
}

long App::getStateVersion()
{
    return this->stateversion;
}

String App::computeUUID()
{
    uint64_t chipid = ESP.getEfuseMac();
    uint32_t high = chipid >> 32;
    uint32_t low = chipid & 0xFFFFFFFF;

    char uuid[37];
    snprintf(uuid, sizeof(uuid), "%08X-%04X-%04X-%04X-%04X%08X",
             (uint32_t)(chipid >> 32),
             (uint16_t)(chipid >> 16 & 0xFFFF),
             (uint16_t)(chipid & 0xFFFF),
             (uint16_t)(high >> 16) | 0x4000,    // Version 4 UUID
             (uint16_t)(high & 0xFFFF) | 0x8000, // Variant 1 UUID
             low);

    return String(uuid);
}

String App::computeSerialNumber()
{
    uint8_t chipId[6];
    esp_efuse_mac_get_default(chipId);

    uint32_t serialNumber = 0;
    for (int i = 0; i < 6; i++)
    {
        serialNumber += (chipId[i] << (8 * i));
    }

    char serialStr[13];
    snprintf(serialStr, sizeof(serialStr), "%012X", serialNumber);

    return String(serialStr);
}

void App::setName(String name)
{
    this->name = name;
}

String App::getName()
{
    return this->name = name;
}

void App::setDeviceType(String devicetype)
{
    this->devicetype = devicetype;
}

String App::computeTechnicalName()
{
    String tn = "" + this->name;
    tn.replace(' ', '_');
    tn.toLowerCase();
    return tn;
}

void App::setVersion(String version)
{
    this->version = version;
}

void App::setManufacturer(String manufacturer)
{
    this->manufacturer = manufacturer;
}

void App::setServerPort(int serverPort)
{
    this->serverPort = serverPort;
}

void App::setMQTTBrokerHost(String mqttBrokerHost)
{
    this->mqttBrokerHost = mqttBrokerHost;
}

void App::setMQTTBrokerUsername(String mqttBrokerUsername)
{
    this->mqttBrokerUsername = mqttBrokerUsername;
}

void App::setMQTTBrokerPassword(String mqttBrokerPassword)
{
    this->mqttBrokerPassword = mqttBrokerPassword;
}

void App::setMQTTBrokerPort(int mqttBrokerPort)
{
    this->mqttBrokerPort = mqttBrokerPort;
}

void App::announceMDNS()
{
    String technicalName = this->computeTechnicalName();
    if (MDNS.begin(technicalName))
    {
        Serial.println("announceMDNS() - Registered as mDNS-Name " + technicalName);
    }
    else
    {
        Serial.println("announceMDNS() - Registered as mDNS-Name " + technicalName + " failed");
    }
}

void App::announceSSDP()
{
    Serial.println("announceSSDP() - Initializing SSDP...");

    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(this->serverPort);
    SSDP.setName(computeTechnicalName());
    SSDP.setURL("/");
    SSDP.setDeviceType(
        "rootdevice"); // to appear as root device, other examples:
                       // MediaRenderer, MediaServer ...
    SSDP.setManufacturer(this->manufacturer);
    SSDP.setModelName(this->devicetype);
    SSDP.setServerName("SSDPServer/1.0");
    SSDP.setModelNumber(this->version);
    SSDP.setSerialNumber(this->computeSerialNumber());

    SSDP.setUUID(this->computeUUID().c_str());

    SSDP.begin();
}

const char *App::getSSDPSchema()
{
    return SSDP.getSchema();
}

void App::MQTT_init()
{
    Serial.println("MQTT_init() - Initializing MQTT client");
    this->pubsubclient->setBufferSize(1024);
    this->pubsubclient->setServer(this->mqttBrokerHost.c_str(), this->mqttBrokerPort);

    String technicalName = this->computeTechnicalName();

    this->pubsubclient->setCallback([this](char *topic, byte *payload, unsigned int length)
                                    {
            String to(topic);
            String messagePayload = String(payload, length);

            for (const auto& func : this->mqttCallbacks) {
                func(to, messagePayload);
             } });

    this->mqttinit = true;

    this->MQTT_reconnect();
}

void App::MQTT_reconnect()
{
    int waitcount = 0;
    while (!this->pubsubclient->connected())
    {
        Serial.print(F("MQTT_reconnect() - Attempting MQTT connection..."));
        // Attempt to connect
        if (!this->pubsubclient->connect(this->computeTechnicalName().c_str(), this->mqttBrokerUsername.c_str(), this->mqttBrokerPassword.c_str()))
        {
            Serial.print(F(" failed, rc="));
            Serial.print(this->pubsubclient->state());
            Serial.println(F(" try again..."));

            waitcount++;
            if (waitcount > 20)
            {
                Serial.println("Giving up, restarting.");
                ESP.restart();
            }

            delay(100);
        }
        else
        {
            Serial.println(F(" ok"));
        }
    }
    String subscribeWildCard = computeTechnicalName() + "/+/set";
    this->pubsubclient->subscribe(subscribeWildCard.c_str());
}

String App::MQTT_announce_button(String buttonId, String title, String icon, const std::function<void()> &clickHandler)
{
    String technicalName = this->computeTechnicalName();
    String objectId = technicalName + "_" + buttonId + "_btn";
    String discoveryTopic = "homeassistant/button/" + objectId + "/config";

    String topicPrefix = technicalName + "/" + buttonId;
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
    device["name"] = this->name;
    device["model"] = this->devicetype;
    device["manufacturer"] = this->manufacturer;
    device["model_id"] = this->version;
    device["serial_number"] = this->computeSerialNumber();
    device["configuration_url"] = "http://" + technicalName + ".local:" + this->serverPort + "/";

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->mqttCallbacks.push_back([commandTopic, clickHandler](String topic, String payload)
                                  {
            if (topic == commandTopic) {
                Serial.println("button() - Got Message on " + commandTopic);
                if (payload == "PRESS") {
                    Serial.println("button() - Pressed!");
                    clickHandler();
                }
            } });

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return stateTopic;
}

String App::MQTT_announce_number(String numberId, String title, String icon, String mode, float min, float max, const std::function<void(String)> &changeHandler)
{
    String technicalName = this->computeTechnicalName();
    String objectId = technicalName + "_" + numberId + "_nbr";
    String discoveryTopic = "homeassistant/number/" + objectId + "/config";

    String topicPrefix = technicalName + "/" + numberId;
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
    device["name"] = this->name;
    device["model"] = this->devicetype;
    device["manufacturer"] = this->manufacturer;
    device["model_id"] = this->version;
    device["serial_number"] = this->computeSerialNumber();
    device["configuration_url"] = "http://" + technicalName + ".local:" + this->serverPort + "/";

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->mqttCallbacks.push_back([commandTopic, changeHandler](String topic, String payload)
                                  {
            if (topic == commandTopic) {
                Serial.println("number() - Changed");
                changeHandler(payload);
            } });

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return stateTopic;
}

String App::MQTT_announce_sensor(String notifyId, String title, String icon)
{
    String technicalName = this->computeTechnicalName();
    String objectId = technicalName + "_" + notifyId + "_sens";
    String discoveryTopic = "homeassistant/sensor/" + objectId + "/config";

    String topicPrefix = technicalName + "/" + notifyId;
    String stateTopic = topicPrefix + "/state";

    JsonDocument discoverypayload;
    discoverypayload["object_id"] = objectId;
    discoverypayload["unique_id"] = objectId;
    discoverypayload["state_topic"] = stateTopic;
    discoverypayload["name"] = title;
    discoverypayload["icon"] = icon;

    JsonObject device = discoverypayload["device"].to<JsonObject>();
    device["name"] = this->name;
    device["model"] = this->devicetype;
    device["manufacturer"] = this->manufacturer;
    device["model_id"] = this->version;
    device["serial_number"] = this->computeSerialNumber();
    device["configuration_url"] = "http://" + technicalName + ".local:" + this->serverPort + "/";

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return stateTopic;
}

String App::MQTT_announce_tagscanner(String notifyId)
{
    String technicalName = this->computeTechnicalName();
    String objectId = technicalName + "_" + notifyId + "_tagscan";
    String discoveryTopic = "homeassistant/tag/" + objectId + "/config";

    String topicPrefix = technicalName + "/" + notifyId;
    String topic = topicPrefix + "/state";

    JsonDocument discoverypayload;
    discoverypayload["object_id"] = objectId;
    discoverypayload["unique_id"] = objectId;
    discoverypayload["topic"] = topic;
    discoverypayload["value_template"] = "{{value_json.UID}}";

    JsonObject device = discoverypayload["device"].to<JsonObject>();
    device["name"] = this->name;
    device["model"] = this->devicetype;
    device["manufacturer"] = this->manufacturer;
    device["model_id"] = this->version;
    device["serial_number"] = this->computeSerialNumber();
    device["configuration_url"] = "http://" + technicalName + ".local:" + this->serverPort + "/";

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());

    return topic;
}

void App::MQTT_publish(String topic, String payload)
{
    if (this->wificonnected && this->mqttinit)
    {
        this->pubsubclient->publish(topic.c_str(), payload.c_str());
    }
}

void App::loop()
{
    if (this->wificonnected && this->mqttinit)
    {
        if (!this->pubsubclient->connected())
        {
            this->MQTT_reconnect();
        }

        this->pubsubclient->loop();
    }
}

void App::writeCommandToTag(CommandData command)
{
    uint8_t userdata[44];
    memcpy(&userdata[0], &command, 44);
    this->tagscanner->write(&userdata[0], 44);
}

void App::clearTag()
{
    this->tagscanner->clearTag();
}

float App::getVolume()
{
    return this->player->volume();
}

bool App::isActive()
{
    return this->player->isActive();
}

const char* App::currentTitle()
{
    return this->source->toStr();
}

void App::setVolume(float volume)
{
    this->player->setVolume(volume);
}

void App::toggleActiveState() 
{
    Serial.println("toggleActiveState() - Toggling player state");
    this->player->setActive(!this->player->isActive());
}

void App::previous()
{
    if (this->source->index() > 0) {
        Serial.println("previous() - Previous title");
        this->player->previous();
    } else {
        Serial.println("previous() - Already at the beginning!");
    }
}

void App::next()
{
    this->player->next();
}

void App::play(String path, int index)
{
    strcpy(this->currentpath, path.c_str());

    this->player->setActive(false);
    this->source->setPath(currentpath);
    this->player->begin(index, true);
    this->player->setActive(true);
}