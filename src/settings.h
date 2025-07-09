#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

#include <FS.h>
#include <vector>

class Settings {
private:
	FS *fs;
	String configurationfilename;
	bool wlan_enabled = WLAN_ENABLED;
	String wlan_sid = WLAN_SID;
	String wlan_pwd = WLAN_PASSWORD;

	bool mqtt_enabled = MQTT_ENABLED;
	String mqtt_server = MQTT_SERVER;
	int mqtt_port = MQTT_PORT;
	String mqtt_username = MQTT_USERNAME;
	String mqtt_password = MQTT_PASSWORD;

	bool voice_enabled = VOICE_ENABLED;
	String voice_server = VOICE_SERVER;
	int voice_port = VOICE_PORT;
	String voice_token = VOICE_TOKEN;

	String deviceName = DEVICENAME;
	std::vector<String> blueoothdeviceprefixes = {BLUETOOTH_DEVICEPREFIX};

public:
	Settings(FS *fs, String configurationfilename);

	bool readFromConfig();
	bool writeToConfig();

	String getDeviceName();

	bool isWiFiEnabled();

	bool isMQTTEnabled();
	String getMQTTServer();
	int getMQTTPort();
	String getMQTTUsername();
	String getMQTTPassword();

	bool isVoiceAssistantEnabled();
	String getVoiceAssistantServer();
	int getVoiceAssistantPort();
	String getVoiceAssistantAccessToken();
	float getVoiceAssistantVolumeMultiplier();
	int getVoiceAssistantWakeWordTimeout();
	int getVoiceAssistantAutomaticGain();
	int getVoiceAssistantNoiseSuppressionLevel();

	void initializeWifiFromSettings();

	void rescanForBetterNetworksAndReconfigure();

	String getSettingsAsJson();
	void setSettingsFromJson(String json);

	bool isValidDeviceToPairForBluetooth(String ssid);

	String computeTechnicalName();
};

#endif
