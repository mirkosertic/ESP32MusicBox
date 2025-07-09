#include "settings.h"

#include "logging.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <WiFi.h>

Settings::Settings(FS *fs, String configurationfilename) {
	this->fs = fs;
	this->configurationfilename = configurationfilename;
}

bool Settings::readFromConfig() {
	File configFile = this->fs->open(configurationfilename, FILE_READ);
	if (!configFile) {
		INFO("No configuration file detected!");
		return false;
	} else {
		JsonDocument document;
		DeserializationError error = deserializeJson(document, configFile);

		configFile.close();

		if (error) {
			WARN("Could not read configuration file!");
			return false;
		} else {
			String data;
			serializeJsonPretty(document, data);

			INFO("Configuration file read : %s", data.c_str());

			JsonObject network = document["network"].as<JsonObject>();
			this->wlan_enabled = network["enabled"].as<bool>();
			this->wlan_sid = String(network["sid"].as<String>());
			this->wlan_pwd = String(network["pwd"].as<String>());

			JsonObject mqtt = document["mqtt"].as<JsonObject>();
			this->mqtt_enabled = mqtt["enabled"].as<bool>();
			this->mqtt_server = String(mqtt["host"].as<String>());
			this->mqtt_port = mqtt["port"].as<int>();
			this->mqtt_username = String(mqtt["user"].as<String>());
			this->mqtt_password = String(mqtt["password"].as<String>());

			JsonObject assistant = document["voiceassistant"].as<JsonObject>();
			this->voice_enabled = assistant["enabled"].as<bool>();
			this->voice_server = String(assistant["host"].as<String>());
			this->voice_port = assistant["port"].as<int>();
			this->voice_token = String(assistant["accesstoken"].as<String>());

			JsonObject device = document["device"].as<JsonObject>();
			this->deviceName = String(device["name"].as<String>());

			return true;
		}
	}
}

bool Settings::writeToConfig() {
	INFO("Writing configuration to FS..");

	JsonDocument doc;
	JsonObject network = doc["network"].to<JsonObject>();
	network["enabled"] = this->wlan_enabled;
	network["sid"] = this->wlan_sid;
	network["pwd"] = this->wlan_pwd;

	JsonObject mqtt = doc["mqtt"].to<JsonObject>();
	mqtt["enabled"] = this->mqtt_enabled;
	mqtt["host"] = this->mqtt_server;
	mqtt["port"] = this->mqtt_port;
	mqtt["user"] = this->mqtt_username;
	mqtt["password"] = this->mqtt_password;

	JsonObject voiceassistant = doc["voiceassistant"].to<JsonObject>();
	voiceassistant["enabled"] = this->voice_enabled;
	voiceassistant["host"] = this->voice_server;
	voiceassistant["port"] = this->voice_port;
	voiceassistant["accesstoken"] = this->voice_token;

	JsonObject device = doc["device"].to<JsonObject>();
	device["name"] = this->deviceName;

	JsonObject bluetooth = doc["bluetooth"].to<JsonObject>();
	JsonArray prefixes = bluetooth["nameprefixes"].to<JsonArray>();
	for (auto str : this->blueoothdeviceprefixes) {
		prefixes.add(str);
	}

	File configFile = this->fs->open(configurationfilename, FILE_WRITE, true);
	if (!configFile) {
		WARN("Could not create config file!");
	} else {
		serializeJson(doc, configFile);
		configFile.close();
	}

	return true;
}

String Settings::getDeviceName() {
	return deviceName;
}

void Settings::initializeWifiFromSettings() {
	if (this->wlan_enabled) {
		WiFi.disconnect();
		INFO("Connecting to WiFi with SID...");
		WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
		WiFi.setHostname(computeTechnicalName().c_str());
		WiFi.begin(this->wlan_sid, this->wlan_pwd);
	} else {
		WARN("WiFi is disabled!");
	}
}

String Settings::getMQTTServer() {
	return this->mqtt_server;
}

int Settings::getMQTTPort() {
	return this->mqtt_port;
}

String Settings::getMQTTUsername() {
	return this->mqtt_username;
}

String Settings::getMQTTPassword() {
	return this->mqtt_password;
}

bool Settings::isMQTTEnabled() {
	return this->mqtt_enabled;
}

bool Settings::isVoiceAssistantEnabled() {
	// return this->voice_enabled;
	return false;
}

String Settings::getVoiceAssistantServer() {
	return this->voice_server;
}

int Settings::getVoiceAssistantPort() {
	return this->voice_port;
}

String Settings::getVoiceAssistantAccessToken() {
	return this->voice_token;
}

float Settings::getVoiceAssistantVolumeMultiplier() {
	return 3.9;
}

int Settings::getVoiceAssistantWakeWordTimeout() {
	return 3;
}

int Settings::getVoiceAssistantAutomaticGain() {
	// Recommended for a quiet microphone
	return 31;
}

int Settings::getVoiceAssistantNoiseSuppressionLevel() {
	// Recommended for a quiet microphone
	return 2;
}

String Settings::getSettingsAsJson() {
	JsonDocument doc;
	JsonObject network = doc["network"].to<JsonObject>();
	network["enabled"] = this->wlan_enabled;
	network["sid"] = this->wlan_sid;
	network["pwd"] = this->wlan_pwd;

	JsonObject mqtt = doc["mqtt"].to<JsonObject>();
	mqtt["enabled"] = this->mqtt_enabled;
	mqtt["host"] = this->mqtt_server;
	mqtt["port"] = this->mqtt_port;
	mqtt["user"] = this->mqtt_username;
	mqtt["password"] = this->mqtt_password;

	JsonObject voiceassistant = doc["voiceassistant"].to<JsonObject>();
	voiceassistant["enabled"] = this->voice_enabled;
	voiceassistant["host"] = this->voice_server;
	voiceassistant["port"] = this->voice_port;
	voiceassistant["accesstoken"] = this->voice_token;

	JsonObject device = doc["device"].to<JsonObject>();
	device["name"] = this->deviceName;

	JsonObject bluetooth = doc["bluetooth"].to<JsonObject>();
	JsonArray prefixes = bluetooth["nameprefixes"].to<JsonArray>();
	for (auto str : this->blueoothdeviceprefixes) {
		prefixes.add(str);
	}

	String result;
	serializeJsonPretty(doc, result);

	return result;
}

void Settings::setSettingsFromJson(String json) {
	JsonDocument document;
	DeserializationError error = deserializeJson(document, json);
	if (error) {
		WARN("Could not parse json data.");
	} else {
		INFO("Updating configuration from JSON.");

		JsonObject network = document["network"].as<JsonObject>();
		this->wlan_enabled = network["enabled"].as<bool>();
		this->wlan_sid = String(network["sid"].as<String>());
		this->wlan_pwd = String(network["pwd"].as<String>());

		JsonObject mqtt = document["mqtt"].as<JsonObject>();
		this->mqtt_enabled = mqtt["enabled"].as<bool>();
		this->mqtt_server = String(mqtt["host"].as<String>());
		this->mqtt_port = mqtt["port"].as<int>();
		this->mqtt_username = String(mqtt["user"].as<String>());
		this->mqtt_password = String(mqtt["password"].as<String>());

		JsonObject assistant = document["voiceassistant"].as<JsonObject>();
		this->voice_enabled = assistant["enabled"].as<bool>();
		this->voice_server = String(assistant["host"].as<String>());
		this->voice_port = assistant["port"].as<int>();
		this->voice_token = String(assistant["accesstoken"].as<String>());

		JsonObject device = document["device"].as<JsonObject>();
		this->deviceName = String(device["name"].as<String>());

		JsonObject bluetooth = document["bluetooth"].to<JsonObject>();
		JsonArray prefixes = bluetooth["nameprefixes"].to<JsonArray>();

		this->blueoothdeviceprefixes.clear();
		for (auto str : prefixes) {
			this->blueoothdeviceprefixes.push_back(str);
		}

		this->writeToConfig();
	}
}

void Settings::rescanForBetterNetworksAndReconfigure() {
	INFO("Scanning for WiFi networks...");

	uint8_t *bssidconnect = WiFi.BSSID();

	int numNetworks = WiFi.scanComplete();
	if (numNetworks == WIFI_SCAN_FAILED) {
		WiFi.scanNetworks(true);
		delay(100);
		numNetworks = WiFi.scanComplete();
	}
	while (numNetworks == WIFI_SCAN_RUNNING) {
		delay(100);
		numNetworks = WiFi.scanComplete();
	}

	if (numNetworks == WIFI_SCAN_FAILED) {
		WARN("Scan failed!");
		WiFi.scanDelete();
		return;
	}

	// int numNetworks = WiFi.scanNetworks();

	if (numNetworks == 0) {
		WARN("No networks found");
		WiFi.scanDelete();
		return;
	}

	int bestRSSI = -100;
	int bestIndex = -1;

	for (int i = 0; i < numNetworks; i++) {
		if (WiFi.SSID(i) == this->wlan_sid && WiFi.RSSI(i) > bestRSSI) {
			bestRSSI = WiFi.RSSI(i);
			bestIndex = i;
		}
	}

	if (bestIndex >= 0) {
		uint8_t *bssid = WiFi.BSSID(bestIndex);

		if (bssid[0] != bssidconnect[0] || bssid[1] != bssidconnect[1] || bssid[2] != bssidconnect[2] || bssid[3] != bssidconnect[3] || bssid[4] != bssidconnect[4] || bssid[5] != bssidconnect[5]) {
			INFO("Better AP found. Reconnecting...");

			WiFi.disconnect();
			WiFi.begin(this->wlan_sid, this->wlan_pwd, WiFi.channel(bestIndex), bssid);

			this->writeToConfig();
		} else {
			INFO("Already configured to best AP!");
		}
	} else {
		INFO("No better AP found");
	}

	WiFi.scanDelete();
}

bool Settings::isWiFiEnabled() {
	return this->wlan_enabled;
}

bool Settings::isValidDeviceToPairForBluetooth(String ssid) {
	for (auto prefix : this->blueoothdeviceprefixes) {
		if (ssid.startsWith(prefix)) {
			INFO("Bluetooth device %s matches configured device prefix %s", ssid.c_str(), prefix.c_str());
			return true;
		}
	}
	return false;
}

String Settings::computeTechnicalName() {
	String tn = "" + this->deviceName;
	tn.replace(' ', '_');
	tn.toLowerCase();
	return tn;
}
