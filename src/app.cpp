#include "app.h"

#include <ESPmDNS.h>
#include <ESP32SSDP.h>
#include <ArduinoJson.h>

#include <esp_system.h>

#include "logging.h"
#include "gitrevision.h"

App::App(WiFiClient &wifiClient, TagScanner *tagscanner, MediaPlayerSource *source, MediaPlayer *player, Settings *settings)
{
    this->tagscanner = tagscanner;
    this->stateversion = 0;
    this->currentpath = new char[512];
    this->tagPresent = false;
    this->tagName = "";
    this->wificonnected = false;

    this->source = source;
    this->player = player;
    this->settings = settings;
}

App::~App()
{
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

String App::getDeviceType()
{
    return this->devicetype;
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

String App::getVersion()
{
    return version;
}

String App::getSoftwareVersion()
{
    return String(gitRevShort);
}

void App::setManufacturer(String manufacturer)
{
    this->manufacturer = manufacturer;
}

String App::getManufacturer()
{
    return this->manufacturer;
}

void App::setServerPort(int serverPort)
{
    this->serverPort = serverPort;
}

String App::getConfigurationURL()
{
    return "http://" + this->computeTechnicalName() + ".local:" + this->serverPort + "/";
}

void App::announceMDNS()
{
    String technicalName = this->computeTechnicalName();
    if (MDNS.begin(technicalName))
    {
        INFO_VAR("Registered as mDNS-Name %s", technicalName.c_str());
    }
    else
    {
        WARN_VAR("Registered as mDNS-Name %s failed", technicalName.c_str());
    }
}

void App::announceSSDP()
{
    INFO("Initializing SSDP...");

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

    if (!SSDP.begin())
    {
        WARN("SSDP init failed!");
    }
    else
    {
        INFO("Done");
    }
}

const char *App::getSSDPSchema()
{
    return SSDP.getSchema();
}

void App::loop()
{
    this->player->copy();
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

const char *App::currentTitle()
{
    return this->source->toStr();
}

void App::setVolume(float volume)
{
    INFO_VAR("Setting volume to %f", volume);
    this->player->setVolume(volume);
}

void App::toggleActiveState()
{
    INFO("Toggling player state");
    this->player->setActive(!this->player->isActive());
}

void App::previous()
{
    if (this->source->index() > 0)
    {
        INFO("Previous title");
        this->player->previous();
    }
    else
    {
        WARN("Already at the beginning!");
    }
}

void App::next()
{
    if (!this->player->next())
    {
        WARN("No more next titles!");
    }
}

void App::play(String path, int index)
{
    INFO_VAR("Playing song in path %s with index %d", path.c_str(), index);
    strcpy(this->currentpath, path.c_str());

    INFO("Player active=false");
    this->player->setActive(false);
    INFO("Setting path");
    this->source->setPath(currentpath);
    INFO_VAR("Playing index %d", index);
    this->player->begin(index, true);
    INFO("Player active=true");
    this->player->setActive(true);
}