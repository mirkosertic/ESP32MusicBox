#include "app.h"

#include "gitrevision.h"
#include "logging.h"

#include <ArduinoJson.h>
#include <esp_mac.h>
#include <esp_system.h>

App::App(Leds *leds, TagScanner *tagscanner, MediaPlayer *player, Settings *settings, BluetoothSource *bluetoothsource, Equalizer3Bands *equalizer) {
	this->leds = leds;
	this->tagscanner = tagscanner;
	this->stateversion = 0;
	this->tagPresent = false;
	this->tagName = "";

	this->player = player;
	this->settings = settings;
	this->bluetoothsource = bluetoothsource;

	this->equalizer = equalizer;
}

App::~App() {
}

void App::begin(ChangeNotifierCallback callback) {
	this->changecallback = callback;
}

bool App::isWifiEnabled() {
	return this->settings->isWiFiEnabled();
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

	if (WiFi.isConnected()) {
		long now = millis();
		if (now - lastStateReport > 5000) {
			DEBUG("Publishing app state");
			publishState();

			unsigned int stackHighWatermark = uxTaskGetStackHighWaterMark(nullptr);

			INFO("Free HEAP is %d, stackHighWatermark is %d", ESP.getFreeHeap(), stackHighWatermark);

			lastStateReport = now;
		}
	}

	this->tagscanner->loop();
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
	return this->player->volume();
}

bool App::isActive() {
	return this->player->isActive();
}

const char *App::currentTitle() {
	return this->player->currentSong();
}

void App::publishState() {
	this->changecallback(this->player->isActive(), this->player->volume(), this->player->currentSong(), this->player->playProgressInPercent());
}

bool App::volumeDown() {

	float volume = this->getVolume();
	if (volume > 0.0) {
		DEBUG("Decrementing volume, old volume %f", volume);

		volume -= settings->getVolumeIncrement();
		if (volume < 0.0) {
			volume = 0.0;
		}

		DEBUG("New volume %f", volume);

		this->leds->setState(VOLUME_CHANGE);
		this->setVolume(volume, false);
		return true;
	}
	return false;
}

bool App::volumeUp() {
	float volume = this->getVolume();
	if (volume < 1.0) {
		DEBUG("Decrementing volume, old volume %f", volume);

		volume += settings->getVolumeIncrement();
		if (volume > 1.0) {
			volume = 1.0;
		}

		DEBUG("New volume %f", volume);

		this->leds->setState(VOLUME_CHANGE);
		this->setVolume(volume, false);
		return true;
	}
	return false;
}

void App::setVolume(float volume, bool publishstate) {
	const std::lock_guard<std::mutex> lock(this->loopmutex);

	DEBUG("Setting volume to %f", volume);
	this->player->setVolume(volume);

	if (publishstate) {
		this->publishState();
	}
}

void App::toggleActiveState() {
	const std::lock_guard<std::mutex> lock(this->loopmutex);

	INFO("Toggling player state");
	this->player->setActive(!this->player->isActive());
	this->publishState();
}

void App::previous() {
	const std::lock_guard<std::mutex> lock(this->loopmutex);
	if (this->player->isActive()) {
		if (this->player->hasPrevious()) {
			INFO("Previous title");
			this->player->previous();

			this->publishState();
		} else {
			WARN("Already at the beginning!");
		}
	} else {
		WARN("Cannot go to previous title as player is not active!");
	}
}

void App::next() {
	const std::lock_guard<std::mutex> lock(this->loopmutex);
	if (this->player->isActive()) {
		if (!this->player->next()) {
			WARN("No more next titles!");
		} else {
			this->publishState();
		}
	} else {
		WARN("Cannot go to next title as player is not active!");
	}
}

void App::play(String path, int index) {
	const std::lock_guard<std::mutex> lock(this->loopmutex);

	this->player->playFromSD(path, index);

	this->publishState();
}

int App::playProgressInPercent() {
	return this->player->playProgressInPercent();
}

void App::playURL(String url) {
	const std::lock_guard<std::mutex> lock(this->loopmutex);

	INFO("Stopping Bluetooth support due to WiFi / BT coexistence problems...")
	this->bluetoothsource->end();

	this->player->playURL(url, false);

	this->publishState();
}

void App::shutdown() {
	INFO("Shutting down everything")
	this->leds->end();
	esp_deep_sleep_start();
}

void App::rgbtest(int r, int g, int b) {
	this->leds->rgbtest(r, g, b);
}

void App::equalizerLow(float value) {
	INFO("Setting equalizer.low to %f", value);
	ConfigEqualizer3Bands &eqconfig = this->equalizer->config();
	eqconfig.gain_low = value;
	this->equalizer->begin(eqconfig);
}

void App::equalizerMiddle(float value) {
	INFO("Setting equalizer.middle to %f", value);
	ConfigEqualizer3Bands &eqconfig = this->equalizer->config();
	eqconfig.gain_medium = value;
	this->equalizer->begin(eqconfig);
}

void App::equalizerHigh(float value) {
	INFO("Setting equalizer.high to %f", value);
	ConfigEqualizer3Bands &eqconfig = this->equalizer->config();
	eqconfig.gain_high = value;
	this->equalizer->begin(eqconfig);
}
