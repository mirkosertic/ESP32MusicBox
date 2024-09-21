#include "settings.h"

#include <FS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include "logging.h"

Settings::Settings(FS *fs, String configurationfilename)
{
  this->fs = fs;
  this->configurationfilename = configurationfilename;
}

bool Settings::readFromConfig()
{
  File configFile = this->fs->open(configurationfilename, FILE_READ);
  if (!configFile)
  {
    INFO("No configuration file detected!");
    return false;
  }
  else
  {
    JsonDocument document;
    DeserializationError error = deserializeJson(document, configFile);

    configFile.close();

    if (error)
    {
      WARN("Could not read configuration file!");
      return false;
    }
    else
    {
      String data;
      serializeJsonPretty(document, data);

      INFO_VAR("Configuration file read : %s", data.c_str());

      JsonObject network = document["network"].as<JsonObject>();
      this->wlan_enabled = network["enabled"].as<bool>();
      this->wlan_sid = String(network["sid"].as<String>());
      this->wlan_pwd = String(network["pwd"].as<String>());
      this->wlan_channel = network["channel"].as<int32_t>();

      JsonArray bssid = network["bssid"].as<JsonArray>();
      this->wlan_bssid[0] = bssid[0].as<uint8_t>();
      this->wlan_bssid[1] = bssid[1].as<uint8_t>();
      this->wlan_bssid[2] = bssid[2].as<uint8_t>();
      this->wlan_bssid[3] = bssid[3].as<uint8_t>();
      this->wlan_bssid[4] = bssid[4].as<uint8_t>();
      this->wlan_bssid[5] = bssid[5].as<uint8_t>();

      JsonObject mqtt = document["mqtt"].as<JsonObject>();
      this->mqtt_enabled = mqtt["enabled"].as<bool>();
      this->mqtt_server = String(mqtt["host"].as<String>());
      this->mqtt_port = mqtt["port"].as<int>();
      this->mqtt_username = String(mqtt["user"].as<String>());
      this->mqtt_password = String(mqtt["password"].as<String>());

      JsonObject assistant = document["voiceassistant"].as<JsonObject>();
      this->voice_enabled = assistant["enabled"].as<bool>();
      this->voice_server = String(assistant["host"].as<String>());
      this->voice_port = mqtt["port"].as<int>();
      this->voice_token = String(assistant["accesstoken"].as<String>());

      return true;
    }
  }
}

bool Settings::writeToConfig()
{
  INFO("Writing configuration to FS..");

  JsonDocument doc;
  JsonObject network = doc["network"].to<JsonObject>();
  network["enabled"] = this->wlan_enabled;
  network["sid"] = this->wlan_sid;
  network["pwd"] = this->wlan_pwd;
  network["channel"] = WiFi.channel();

  uint8_t *bssid = WiFi.BSSID();
  JsonArray bs = network["bssid"].to<JsonArray>();
  bs.add(bssid[0]);
  bs.add(bssid[1]);
  bs.add(bssid[2]);
  bs.add(bssid[3]);
  bs.add(bssid[4]);
  bs.add(bssid[5]);

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

  File configFile = this->fs->open(configurationfilename, FILE_WRITE, true);
  if (!configFile)
  {
    WARN("Could not create config file!");
  }
  else
  {
    serializeJson(doc, configFile);
    configFile.close();
  }

  return true;
}

void Settings::initializeWifiFromSettings()
{
  if (this->wlan_enabled)
  {
    WiFi.disconnect();
    if (this->wlan_bssid[0] == 0 && this->wlan_bssid[1] == 0 && this->wlan_bssid[2] == 0 && this->wlan_bssid[3] == 0 && this->wlan_bssid[4] == 0 && this->wlan_bssid[5] == 0)
    {
      INFO("Connecting to Wifi with SID...");
      WiFi.begin(this->wlan_sid, this->wlan_pwd);
    }
    else
    {
      INFO("Connecting to Wifi with last known AP...");
      WiFi.begin(this->wlan_sid, this->wlan_pwd, this->wlan_channel, this->wlan_bssid);
    }
  }
  else
  {
    WARN("WiFi is disabled!");
  }
}

String Settings::getMQTTServer()
{
  return this->mqtt_server;
}

int Settings::getMQTTPort()
{
  return this->mqtt_port;
}

String Settings::getMQTTUsername()
{
  return this->mqtt_username;
}

String Settings::getMQTTPassword()
{
  return this->mqtt_password;
}

bool Settings::isMQTTEnabled()
{
  return this->mqtt_enabled;
}

bool Settings::isVoiceAssistantEnabled()
{
  return this->voice_enabled;
}

String Settings::getVoiceAssistantServer()
{
  return this->voice_server;
}

int Settings::getVoiceAssistantPort()
{
  return this->voice_port;
}

String Settings::getVoiceAssistantAccessToken()
{
  return this->voice_token;
}

String Settings::getSettingsAsJson()
{
  JsonDocument doc;
  JsonObject network = doc["network"].to<JsonObject>();
  network["enabled"] = this->wlan_enabled;
  network["sid"] = this->wlan_sid;
  network["pwd"] = this->wlan_pwd;
  network["channel"] = WiFi.channel();

  JsonArray bs = network["bssid"].to<JsonArray>();
  bs.add(this->wlan_bssid[0]);
  bs.add(this->wlan_bssid[1]);
  bs.add(this->wlan_bssid[2]);
  bs.add(this->wlan_bssid[3]);
  bs.add(this->wlan_bssid[4]);
  bs.add(this->wlan_bssid[5]);

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

  String result;
  serializeJsonPretty(doc, result);

  return result;
}

void Settings::setSettingsFromJson(String json)
{
  JsonDocument document;
  DeserializationError error = deserializeJson(document, json);
  if (error)
  {
    WARN("Could not parse json data.");
  }
  else
  {
    INFO("Updating configuration from JSON.");

    JsonObject network = document["network"].as<JsonObject>();
    this->wlan_enabled = network["enabled"].as<bool>();
    this->wlan_sid = String(network["sid"].as<String>());
    this->wlan_pwd = String(network["pwd"].as<String>());
    this->wlan_channel = network["channel"].as<int32_t>();

    JsonArray bssid = network["bssid"].as<JsonArray>();
    this->wlan_bssid[0] = bssid[0].as<uint8_t>();
    this->wlan_bssid[1] = bssid[1].as<uint8_t>();
    this->wlan_bssid[2] = bssid[2].as<uint8_t>();
    this->wlan_bssid[3] = bssid[3].as<uint8_t>();
    this->wlan_bssid[4] = bssid[4].as<uint8_t>();
    this->wlan_bssid[5] = bssid[5].as<uint8_t>();

    JsonObject mqtt = document["mqtt"].as<JsonObject>();
    this->mqtt_enabled = mqtt["enabled"].as<bool>();
    this->mqtt_server = String(mqtt["host"].as<String>());
    this->mqtt_port = mqtt["port"].as<int>();
    this->mqtt_username = String(mqtt["user"].as<String>());
    this->mqtt_password = String(mqtt["password"].as<String>());

    JsonObject assistant = document["voiceassistant"].as<JsonObject>();
    this->voice_enabled = assistant["enabled"].as<bool>();
    this->voice_server = String(assistant["host"].as<String>());
    this->voice_port = mqtt["port"].as<int>();
    this->voice_token = String(assistant["accesstoken"].as<String>());

    this->writeToConfig();
  }
}

void Settings::rescanForBetterNetworksAndReconfigure()
{
  INFO("Scanning for WiFi networks...");
  int numNetworks = WiFi.scanComplete();
  if (numNetworks == WIFI_SCAN_FAILED)
  {
    WiFi.scanNetworks(true);
    delay(100);
    numNetworks = WiFi.scanComplete();
  }
  while (numNetworks == WIFI_SCAN_RUNNING)
  {
    delay(100);    
    numNetworks = WiFi.scanComplete();
  }

  if (numNetworks == WIFI_SCAN_FAILED)
  {
    WARN("Scan failed!");
    WiFi.scanDelete();
    return;
  }

  // int numNetworks = WiFi.scanNetworks();

  if (numNetworks == 0)
  {
    WARN("No networks found");
    WiFi.scanDelete();
    return;
  }

  int bestRSSI = -100;
  int bestIndex = -1;

  for (int i = 0; i < numNetworks; i++)
  {
    if (WiFi.SSID(i) == this->wlan_sid && WiFi.RSSI(i) > bestRSSI)
    {
      bestRSSI = WiFi.RSSI(i);
      bestIndex = i;
    }
  }

  if (bestIndex >= 0)
  {
    uint8_t *bssid = WiFi.BSSID(bestIndex);

    if (bssid[0] != this->wlan_bssid[0] && bssid[1] != this->wlan_bssid[1] && bssid[2] != this->wlan_bssid[2] && bssid[3] != this->wlan_bssid[3] && bssid[4] != this->wlan_bssid[4] && bssid[5] != this->wlan_bssid[5])
    {
      INFO("Better AP found. Reconnecting...");

      uint8_t *bssid = WiFi.BSSID(bestIndex);
      this->wlan_channel = WiFi.channel(bestIndex);
      this->wlan_bssid[0] = bssid[0];
      this->wlan_bssid[1] = bssid[1];
      this->wlan_bssid[2] = bssid[2];
      this->wlan_bssid[3] = bssid[3];
      this->wlan_bssid[4] = bssid[4];
      this->wlan_bssid[5] = bssid[5];

      WiFi.disconnect();
      WiFi.begin(this->wlan_sid, this->wlan_pwd, this->wlan_channel, this->wlan_bssid);

      this->writeToConfig();
    }
    else
    {
      INFO("Already configured to best AP!");
    }
  }
  else
  {
    INFO("No better AP found");
  }

  WiFi.scanDelete();
}

bool Settings::isWiFiEnabled()
{
  return this->wlan_enabled;
}