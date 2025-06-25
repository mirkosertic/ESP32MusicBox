#include "bluetoothsink.h"

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "logging.h"

BluetoothSink::BluetoothSink(I2SStream *output, BluetoothSinkConnectCallback connectCallback)
{
    this->out = output;
    this->connectCallback = connectCallback;

    this->sink = new BluetoothA2DPSink(*output);
    this->sink->clean_last_connection();
    this->sink->set_auto_reconnect(false);
    this->sink->activate_pin_code(true);
    this->sink->set_on_connection_state_changed([](esp_a2d_connection_state_t state, void *x)
                                                {
                                                    BluetoothSink *obj = (BluetoothSink *)x;
                                                    obj->connectCallback(obj, state); },
                                                this);
}

void BluetoothSink::start(String bluetoothSSID)
{
    this->sink->start(bluetoothSSID.c_str());
}

int BluetoothSink::pinCode()
{
    return this->sink->pin_code();
}

void BluetoothSink::confirmPinCode()
{
    static uint32_t lastChecked = 0;
    uint32_t now = millis();
    if (now - lastChecked > 2000)
    {
        INFO("Confirming Bluetooth connection");
        this->sink->confirm_pin_code();
        lastChecked = now;
    }
}

void BluetoothSink::play()
{
    INFO("Playing Bluetooth device");
    this->sink->play();
}

void BluetoothSink::pause()
{
    INFO("Pausing Bluetooth device")
    this->sink->pause();
}

void BluetoothSink::previous()
{
    INFO("Sending Previous command to Bluetooth device");
    this->sink->previous();
}

void BluetoothSink::next()
{
    INFO("Sending Next command to Bluetooth device");
    this->sink->next();
}

void BluetoothSink::volumeUp()
{
    INFO("Sending volume up down to Bluetooth device");
    this->sink->volume_up();
}

void BluetoothSink::volumeDown()
{
    INFO("Sending volume down to Bluetooth device");
    this->sink->volume_down();
}
