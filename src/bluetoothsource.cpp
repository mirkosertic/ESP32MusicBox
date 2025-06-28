#include "bluetoothsource.h"

#include "app.h"
#include "logging.h"

BluetoothSource::BluetoothSource(BluetoothSourceConnectCallback connectCallback,
	SSIDFilterCallback ssidFilterCallback,
	AVRCCallback avrcCallback,
	ReadDataCallback readDataCallback) {

	this->connectCallback = connectCallback;

	INFO("Bluetooth initializing A2DP source. Free HEAP is %d", ESP.getFreeHeap());
	this->source = new BluetoothA2DPSource();
	this->source->clean_last_connection();
	this->source->set_reset_ble(true);
	this->source->set_auto_reconnect(false);
	this->source->set_ssid_callback(ssidFilterCallback);
	this->source->set_discovery_mode_callback([](esp_bt_gap_discovery_state_t discoveryMode) {
    switch (discoveryMode)
    {
    case ESP_BT_GAP_DISCOVERY_STARTED:
    INFO("bluetooth() - Discovery started");
    break;
    case ESP_BT_GAP_DISCOVERY_STOPPED:
    INFO("bluetooth() - Discovery stopped");
    break;
    } });

	this->source->set_data_callback(readDataCallback);

	this->source->set_on_connection_state_changed([](esp_a2d_connection_state_t state, void *x) {
                                                  BluetoothSource *obj = (BluetoothSource *)x;
                                                  obj->connectCallback(obj, state); },
		this);

	this->source->set_avrc_passthru_command_callback(avrcCallback);
}

void BluetoothSource::start(String name) {
	this->source->set_local_name(name.c_str());

	this->source->start();
}
