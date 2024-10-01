#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <FS.h>

class Settings
{
private:
    FS *fs;
    String configurationfilename;
    bool wlan_enabled = WLAN_ENABLED;
    String wlan_sid = WLAN_SID;
    String wlan_pwd = WLAN_PASSWORD;
    int32_t wlan_channel = 1;
    uint8_t wlan_bssid[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    bool mqtt_enabled = MQTT_ENABLED;
    String mqtt_server = MQTT_SERVER;
    int mqtt_port = MQTT_PORT;
    String mqtt_username = MQTT_USERNAME;
    String mqtt_password = MQTT_PASSWORD;

    bool voice_enabled = VOICE_ENABLED;
    String voice_server = VOICE_SERVER;
    int voice_port = VOICE_PORT;
    String voice_token = VOICE_TOKEN;

public:
    Settings(FS *fs, String configurationfilename);

    bool readFromConfig();
    bool writeToConfig();

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

    void resetStoredBSSID();
    void resetStoredBSSIDAndReconfigureWiFi();
    void rescanForBetterNetworksAndReconfigure();

    String getSettingsAsJson();
    void setSettingsFromJson(String json);
};

#endif