#ifndef BLUETOOTHSOURCE_H
#define BLUETOOTHSOURCE_H

#include <AudioTools.h>
#include <BluetoothA2DPSource.h>
#include <functional>

class BluetoothSource;

typedef std::function<void(BluetoothSource *source, esp_a2d_connection_state_t state)> BluetoothSourceConnectCallback;
typedef bool (*SSIDFilterCallback)(const char *ssid, esp_bd_addr_t address, int rrsi);
typedef void (*AVRCCallback)(uint8_t key, bool isReleased);
typedef int32_t (*ReadDataCallback)(uint8_t *data, int32_t len);

class BluetoothSource {
private:
	BluetoothA2DPSource *source;
	BluetoothSourceConnectCallback connectCallback;

public:
	BluetoothSource(BluetoothSourceConnectCallback connectCallback,
		SSIDFilterCallback ssidFilterCallback,
		AVRCCallback avrcCallback,
		ReadDataCallback readDataCallback);
	void start(String name);

	void end();
};

#endif
