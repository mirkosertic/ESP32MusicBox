#include "app.h"

#include "gitrevision.h"
#include "logging.h"

#include <ArduinoJson.h>
#include <esp_mac.h>
#include <esp_system.h>

App::App(Leds *leds, TagScanner *tagscanner, MediaPlayerSource *source, MediaPlayer *player, Settings *settings, VolumeSupport *volumeSupport) {
	this->leds = leds;
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
}

App::~App() {
}

void App::begin(ChangeNotifierCallback callback) {
	this->changecallback = callback;
}

void App::setWifiConnected() {
	this->wificonnected = true;
}

bool App::isWifiEnabled() {
	return this->settings->isWiFiEnabled();
}

bool App::isWifiConnected() {
	return this->wificonnected;
}

void App::noTagPresent() {
	if (this->tagPresent) {
		this->tagPresent = false;
		this->knownTag = false;
		this->tagName = "";
		this->tagData = TagData();

		this->incrementStateVersion();
	}
}

void App::setTagData(bool knownTag, String tagName, uint8_t *uid, uint8_t uidLength, TagData tagData) {
	if (this->tagName != tagName || !this->tagPresent) {
		this->tagPresent = true;
		this->knownTag = knownTag;
		this->tagName = tagName;
		this->tagData = tagData;

		this->incrementStateVersion();
	}
}

bool App::getTagPresent() {
	return this->tagPresent;
}

String App::getTagName() {
	return this->tagName;
}

bool App::getIsKnownTag() {
	return this->knownTag;
}

String App::getTagInfoText() {
	if (this->knownTag) {
		CommandData command;
		memcpy(&command, &this->tagData.data[0], 44);
		if (command.version == COMMAND_VERSION) {
			if (command.command == COMMAND_PLAY_DIRECTORY) {
				String info = "Play directory ";
				info += String((char *) &command.path[0]);
				info += String(" with Volume ");
				info += command.volume;

				return info;
			}
			return String("Unknown command : ") + command.command;
		}
		return String("Unknown version : ") + command.version;
	} else {
		return "";
	}
}

char *App::getCurrentPath() {
	return this->currentpath;
}

void App::incrementStateVersion() {
	this->stateversion = this->stateversion + 1;
}

long App::getStateVersion() {
	return this->stateversion;
}

String App::computeUUID() {
	uint64_t chipid = ESP.getEfuseMac();
	uint32_t high = chipid >> 32;
	uint32_t low = chipid & 0xFFFFFFFF;

	char uuid[37];
	snprintf(uuid, sizeof(uuid), "%08X-%04X-%04X-%04X-%04X%08X",
		(uint32_t) (chipid >> 32),
		(uint16_t) (chipid >> 16 & 0xFFFF),
		(uint16_t) (chipid & 0xFFFF),
		(uint16_t) (high >> 16) | 0x4000, // Version 4 UUID
		(uint16_t) (high & 0xFFFF) | 0x8000, // Variant 1 UUID
		low);

	return String(uuid);
}

String App::computeSerialNumber() {
	uint8_t chipId[6];
	esp_efuse_mac_get_default(chipId);

	uint32_t serialNumber = 0;
	for (int i = 0; i < 6; i++) {
		serialNumber += (chipId[i] << (8 * i));
	}

	char serialStr[13];
	snprintf(serialStr, sizeof(serialStr), "%012X", serialNumber);

	return String(serialStr);
}

void App::setName(String name) {
	this->name = name;
}

String App::getName() {
	return this->name = name;
}

void App::setDeviceType(String devicetype) {
	this->devicetype = devicetype;
}

String App::getDeviceType() {
	return this->devicetype;
}

String App::computeTechnicalName() {
	return this->settings->computeTechnicalName();
}

void App::setVersion(String version) {
	this->version = version;
}

String App::getVersion() {
	return version;
}

String App::getSoftwareVersion() {
	return String(gitRevShort);
}

void App::setManufacturer(String manufacturer) {
	this->manufacturer = manufacturer;
}

String App::getManufacturer() {
	return this->manufacturer;
}

void App::setServerPort(int serverPort) {
	this->serverPort = serverPort;
}

void App::loop() {
	const std::lock_guard<std::mutex> lock(this->loopmutex);
	this->player->copy();

	static long lastStateReport = 0;

	if (wificonnected) {
		long now = millis();
		if (now - lastStateReport > 5000) {
			DEBUG("Publishing app state");
			publishState();

			unsigned int stackHighWatermark = uxTaskGetStackHighWaterMark(nullptr);

			INFO("loop() - Free HEAP is %d, stackHighWatermark is %d", ESP.getFreeHeap(), stackHighWatermark);

			lastStateReport = now;
		}
	}
}

void App::writeCommandToTag(CommandData command) {
	uint8_t userdata[44];
	memcpy(&userdata[0], &command, 44);
	this->tagscanner->write(&userdata[0], 44);

	this->leds->setState(CARD_DETECTED);
}

void App::clearTag() {
	this->tagscanner->clearTag();
}

float App::getVolume() {
	return this->volumeSupport->volume();
}

bool App::isActive() {
	return this->player->isActive();
}

const char *App::currentTitle() {
	return this->source->currentPlayFile();
}

void App::publishState() {
	this->changecallback(this->player->isActive(), this->volumeSupport->volume(), this->player->currentSong(), this->player->playProgressInPercent());
}

bool App::volumeDown() {

	float volume = this->getVolume();
	if (volume >= 0.02) {
		INFO("Decrementing volume");
		this->leds->setState(VOLUME_CHANGE);
		this->setVolume(volume - 0.02);
		return true;
	}
	return false;
}

bool App::volumeUp() {
	float volume = this->getVolume();
	if (volume <= 0.98) {
		INFO("Incrementing volume");
		this->leds->setState(VOLUME_CHANGE);
		this->setVolume(volume + 0.02);
		return true;
	}
	return false;
}

void App::setVolume(float volume) {
	const std::lock_guard<std::mutex> lock(this->loopmutex);

	INFO("Setting volume to %f", volume);
	this->volumeSupport->setVolume(volume);

	this->publishState();
}

void App::toggleActiveState() {
	const std::lock_guard<std::mutex> lock(this->loopmutex);

	INFO("Toggling player state");
	this->player->setActive(!this->player->isActive());
	this->publishState();
}

void App::previous() {
	const std::lock_guard<std::mutex> lock(this->loopmutex);
	if (this->source->index() > 0) {
		INFO("Previous title");
		this->player->previous();

		this->publishState();
	} else {
		WARN("Already at the beginning!");
	}
}

void App::next() {
	const std::lock_guard<std::mutex> lock(this->loopmutex);
	if (!this->player->next()) {
		WARN("No more next titles!");
	} else {
		this->publishState();
	}
}

void App::play(String path, int index) {
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

int App::playProgressInPercent() {
	return this->source->playProgressInPercent();
}
