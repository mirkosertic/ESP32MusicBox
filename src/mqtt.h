#ifndef MQTT_H
#define MQTT_H

#include "settings.h"

#include "app.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <vector>

typedef std::function<void(String, String)> MQTTCallback;

class MQTT {
private:
	App *app;
	PubSubClient *pubsubclient;
	String clientid;
	String host;
	int port;
	String username;
	String password;
	std::vector<MQTTCallback> mqttCallbacks;

	String tagtopic;
	String tagscannertopic;
	String currentsongtopic;
	String playbackstatetopic;
	String volumestatetopic;
	String playprogressstatetopic;
	String voltagetopic;

	String wifiqualitytopic;

	bool initialized;

	String configurationUrl;

	String announceButton(String buttonId, String title, String icon, const std::function<void()> &clickHandler);

	String announceNumber(String numberId, String title, String icon, String mode, float min, float max, const std::function<void(String)> &changeHandler);

	String announceSensor(String notifyId, String title, String icon, String deviceclass = "", String displayUnit = "");

	String announceTagscanner(String notifyId);

	void performAutoDiscovery();

	void publish(String topic, String payload);

public:
	MQTT(WiFiClient &wifiClient, App *app);
	~MQTT();

	void begin(String host, int port, String username, String password, String configurationUrl);

	void loop();

	void publishCurrentSong(String value);

	void publishPlaybackState(String value);

	void publishVolume(int value);

	void publishScannedTag(String value);

	void publishTagScannerInfo(String value);

	void publishWiFiQuality(int rssi);

	void publishPlayProgress(int progressInPercent);

	void publishBatteryVoltage(int voltage);
};

#endif
