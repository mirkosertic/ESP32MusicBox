#include "bluetoothsink.h"

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "logging.h"
#include "leds.h"
#include "app.h"

BluetoothA2DPSink *globalSink;

BluetoothSink::BluetoothSink(I2SStream *output, Leds *leds, App *app)
{
    this->out = output;
    this->leds = leds;
    this->app = app;

    this->sink = new BluetoothA2DPSink(*output);
    this->sink->clean_last_connection();
    this->sink->set_auto_reconnect(false);
    this->sink->activate_pin_code(true);
    this->sink->set_on_connection_state_changed([](esp_a2d_connection_state_t state, void *x)
                                                {
        BluetoothSink *obj = (BluetoothSink *) x;
        switch (state)
        {
            case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                INFO("bluetooth() - DISCONNECTED");
                break;
            case ESP_A2D_CONNECTION_STATE_CONNECTING:
                INFO("bluetooth() - CONNECTING. Wairing for Pairing to finish...");
                obj->leds->setState(BTCONNECTING);
                break;
            case ESP_A2D_CONNECTION_STATE_CONNECTED:
                INFO("bluetooth() - CONNECTED. Receiving data from Bluetooth source");
                obj->leds->setState(BTCONNECTED);
                obj->app->setBluetoothSpeakerConnected();
                obj->leds->setBluetoothSpeakerConnected();
                break;
            case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
                INFO("bluetooth() - DISCONNECTING");
                break;
        } }, this);

    globalSink = this->sink;
}

void BluetoothSink::start()
{
    this->sink->start(app->computeTechnicalName().c_str());
    this->app->actAsBluetoothSpeaker(this);
}

int BluetoothSink::pinCode()
{
    return this->sink->pin_code();
}

void BluetoothSink::confirmPinCode()
{
    this->sink->debounce([]()
                         { 
        INFO("Confirming Bluetooth-Connection!"); 
        globalSink->confirm_pin_code(); }, 5000);
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
