#include "app.h"

#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include <esp_system.h>
#include <esp_mac.h>

#include "logging.h"
#include "bluetoothsink.h"
#include "gitrevision.h"

App::App(TagScanner *tagscanner, MediaPlayerSource *source, MediaPlayer *player, Settings *settings, VolumeSupport *volumeSupport)
{
    this->volumeSupport = volumeSupport;
    this->tagscanner = tagscanner;
    this->stateversion = 0;
    this->currentpath = new char[512];
    this->tagPresent = false;
    this->tagName = "";
    this->wificonnected = false;

    this->source = source;
    this->player = player;
    this->settings = settings;

    this->volume = 1.0f;

    this->btspeakerconnected = false;
    this->actasbluetoothspeaker = false;
    this->bluetoothsink = NULL;
    this->btpause = false;
}

App::~App()
{
}

void App::begin(ChangeNotifierCallback callback)
{
    this->changecallback = callback;
}

void App::setWifiConnected()
{
    this->wificonnected = true;
}

bool App::isWifiEnabled()
{
    return this->settings->isWiFiEnabled();
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
        INFO("Registered as mDNS-Name %s", technicalName.c_str());
    }
    else
    {
        WARN("Registered as mDNS-Name %s failed", technicalName.c_str());
    }
}

void App::ssdpNotify()
{
    INFO("Sent SSDP NOTIFY messages");

    const IPAddress SSDP_MULTICAST_ADDR(239, 255, 255, 250);
    const uint16_t SSDP_PORT = 1900;

    String deviceUUID = computeUUID();

    // Send initial NOTIFY messages
    char notify[1024];

    // Send NOTIFY for different device types
    String deviceTypes[] = {
        "upnp:rootdevice",
        "urn:schemas-upnp-org:device:Basic:1",
        String("uuid:") + deviceUUID};

    const char *SSDP_NOTIFY_TEMPLATE =
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%d/description.xml\r\n"
        "SERVER: SSDPServer/1.0\r\n"
        "NT: %s\r\n"
        "USN: uuid:%s::%s\r\n"
        "NTS: ssdp:alive\r\n"
        "BOOTID.UPNP.ORG: 1\r\n"
        "CONFIGID.UPNP.ORG: 1\r\n"
        "\r\n";

    for (int i = 0; i < 3; i++)
    {
        sprintf(notify, SSDP_NOTIFY_TEMPLATE,
                WiFi.localIP().toString().c_str(),
                this->serverPort,
                deviceTypes[i].c_str(),
                deviceUUID.c_str(),
                deviceTypes[i].c_str());

        this->udp->beginPacket(SSDP_MULTICAST_ADDR, SSDP_PORT);
        this->udp->write((uint8_t *)notify, strlen(notify));
        this->udp->endPacket();
    }
}

void App::announceSSDP()
{
    INFO("Initializing SSDP...");

    const IPAddress SSDP_MULTICAST_ADDR(239, 255, 255, 250);
    const uint16_t SSDP_PORT = 1900;

    this->udp = new WiFiUDP();

    // Join multicast group for SSDP
    if (this->udp->beginMulticast(SSDP_MULTICAST_ADDR, SSDP_PORT))
    {
        INFO("SSDP multicast joined successfully");
    }
    else
    {
        WARN("Failed to join SSDP multicast group");
    }
}

String App::getSSDPDescription()
{
    String deviceUUID = computeUUID();

    String xml = "<?xml version='1.0'?>";
    xml += "<root xmlns='urn:schemas-upnp-org:device-1-0'>";
    xml += "<specVersion><major>1</major><minor>0</minor></specVersion>";
    xml += "<device>";
    xml += "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>";
    xml += "<friendlyName>" + this->name + "</friendlyName>";
    xml += "<manufacturer>" + this->manufacturer + "</manufacturer>";
    xml += "<modelName>" + this->devicetype + "</modelName>";
    xml += "<modelNumber>" + this->version + "</modelNumber>";
    xml += "<serialNumber>" + computeSerialNumber() + "</serialNumber>";
    xml += "<UDN>uuid:" + computeUUID() + "</UDN>";
    xml += "<presentationURL>" + getConfigurationURL() + "</presentationURL>";
    xml += "</device>";
    xml += "</root>";
    return xml;
}

void App::loop()
{
    const std::lock_guard<std::mutex> lock(this->loopmutex);
    this->player->copy();

    static long lastStateReport = 0;

    if (wificonnected)
    {
        long now = millis();
        if (now - lastStateReport > 5000)
        {
            DEBUG("Publishing app state");
            publishState();

            unsigned int stackHighWatermark = uxTaskGetStackHighWaterMark(nullptr);

            INFO("loop() - Free HEAP is %d, stackHighWatermark is %d", ESP.getFreeHeap(), stackHighWatermark);

            lastStateReport = now;

            ssdpNotify();
        }

        // SSDP

        // Incoming messages
        int packetSize = this->udp->parsePacket();
        if (packetSize)
        {
            char packetBuffer[512];
            int len = this->udp->read(packetBuffer, sizeof(packetBuffer) - 1);
            if (len > 0)
            {
                packetBuffer[len] = '\0';

                // Check if it's an M-SEARCH request
                if (strstr(packetBuffer, "M-SEARCH") && strstr(packetBuffer, "ssdp:discover"))
                {
                    INFO("Received SSDP M-SEARCH request");

                    // Extract search target
                    char *stLine = strstr(packetBuffer, "ST:");
                    String searchTarget = "upnp:rootdevice"; // Default

                    if (stLine)
                    {
                        stLine += 3; // Skip "ST:"
                        while (*stLine == ' ')
                            stLine++; // Skip spaces
                        char *end = strstr(stLine, "\r");
                        if (end)
                        {
                            *end = '\0';
                            searchTarget = String(stLine);
                        }
                    }

                    char response[1024];
                    char dateStr[64];

                    // Simple date string (could be improved with real time)
                    sprintf(dateStr, "Mon, 01 Jan 1970 00:00:00 GMT");

                    String deviceUUID = computeUUID();

                    const char *SSDP_RESPONSE_TEMPLATE =
                        "HTTP/1.1 200 OK\r\n"
                        "CACHE-CONTROL: max-age=1800\r\n"
                        "DATE: %s\r\n"
                        "EXT:\r\n"
                        "LOCATION: http://%s:%d/description.xml\r\n"
                        "SERVER: SSDPServer/1.0\r\n"
                        "ST: %s\r\n"
                        "USN: uuid:%s::%s\r\n"
                        "BOOTID.UPNP.ORG: 1\r\n"
                        "CONFIGID.UPNP.ORG: 1\r\n"
                        "\r\n";

                    sprintf(response, SSDP_RESPONSE_TEMPLATE,
                            dateStr,
                            WiFi.localIP().toString().c_str(),
                            this->serverPort,
                            searchTarget.c_str(),
                            deviceUUID.c_str(),
                            searchTarget.c_str());

                    // Send unicast response to requester
                    this->udp->beginPacket(this->udp->remoteIP(), this->udp->remotePort());
                    this->udp->write((uint8_t *)response, strlen(response));
                    this->udp->endPacket();

                    INFO("Sent SSDP response");
                }
            }
        }
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
    return volume;
}

bool App::isActive()
{
    return this->player->isActive();
}

const char *App::currentTitle()
{
    return this->source->currentPlayFile();
}

void App::publishState()
{
    if (!this->actasbluetoothspeaker)
    {
        this->changecallback(this->player->isActive(), this->volumeSupport->volume(), this->player->currentSong(), this->player->playProgressInPercent());
    }
}

bool App::volumeDown()
{
    if (this->actasbluetoothspeaker)
    {
        this->bluetoothsink->volumeDown();
        return true;
    }

    float volume = this->getVolume();
    if (volume >= 0.02)
    {
        INFO("Decrementing volume");
        this->setVolume(volume - 0.02);
        return true;
    }
    return false;
}

bool App::volumeUp()
{
    if (this->actasbluetoothspeaker)
    {
        this->bluetoothsink->volumeUp();
        return true;
    }

    float volume = this->getVolume();
    if (volume <= 0.98)
    {
        INFO("Incrementing volume");
        this->setVolume(volume + 0.02);
        return true;
    }
    return false;
}

void App::setVolume(float volume)
{
    const std::lock_guard<std::mutex> lock(this->loopmutex);

    INFO("Setting volume to %f", volume);
    this->volumeSupport->setVolume(volume);
    this->volume = volume;

    this->publishState();
}

void App::toggleActiveState()
{
    const std::lock_guard<std::mutex> lock(this->loopmutex);

    if (this->actasbluetoothspeaker)
    {
        if (!this->btpause)
        {
            this->btpause = true;
            this->bluetoothsink->pause();
        }
        else
        {
            this->btpause = false;
            this->bluetoothsink->play();
        }
        return;
    }
    INFO("Toggling player state");
    this->player->setActive(!this->player->isActive());
    this->publishState();
}

void App::previous()
{
    const std::lock_guard<std::mutex> lock(this->loopmutex);
    if (this->actasbluetoothspeaker)
    {
        this->bluetoothsink->previous();
    }
    else
    {
        if (this->source->index() > 0)
        {
            INFO("Previous title");
            this->player->previous();

            this->publishState();
        }
        else
        {
            WARN("Already at the beginning!");
        }
    }
}

void App::next()
{
    const std::lock_guard<std::mutex> lock(this->loopmutex);
    if (this->actasbluetoothspeaker)
    {
        this->bluetoothsink->next();
    }
    else
    {
        if (!this->player->next())
        {
            WARN("No more next titles!");
        }
        else
        {
            this->publishState();
        }
    }
}

void App::play(String path, int index)
{
    const std::lock_guard<std::mutex> lock(this->loopmutex);

    INFO("Playing song in path %s with index %d", path.c_str(), index);
    strcpy(this->currentpath, path.c_str());

    INFO("Player active=false");
    this->player->setActive(false);
    INFO("Setting path");
    this->source->setPath(currentpath);
    INFO("Playing index %d", index);
    this->player->begin(index, true);

    this->publishState();
}

int App::playProgressInPercent()
{
    return this->source->playProgressInPercent();
}

void App::setBluetoothSpeakerConnected(bool value)
{
    this->btspeakerconnected = value;
}

bool App::isBluetoothSpeakerConnected()
{
    return this->btspeakerconnected;
}

void App::actAsBluetoothSpeaker(BluetoothSink *bluetoothsink)
{
    this->bluetoothsink = bluetoothsink;
    this->actasbluetoothspeaker = true;
}

bool App::isActAsBluetoothSpeaker()
{
    return this->actasbluetoothspeaker;
}

bool App::isValidDeviceToPairForBluetooth(String ssid)
{
    return this->settings->isValidDeviceToPairForBluetooth(ssid);
}
