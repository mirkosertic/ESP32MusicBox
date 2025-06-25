#ifndef BLUETOOTHSINK_H
#define BLUETOOTHSINK_H

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#include <functional>

#include "leds.h"

class BluetoothSink;

typedef std::function<void(BluetoothSink *sink, esp_a2d_connection_state_t state)> BluetoothSinkConnectCallback;

class BluetoothSink
{
private:
    I2SStream *out;
    BluetoothA2DPSink *sink;
    BluetoothSinkConnectCallback connectCallback;

public:
    BluetoothSink(I2SStream *output, BluetoothSinkConnectCallback connectCallback);

    void start(String bluetoothSSID);

    int pinCode();
    void confirmPinCode();

    void play();
    void pause();
    void previous();
    void next();
    void volumeUp();
    void volumeDown();
};

#endif