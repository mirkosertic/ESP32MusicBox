#ifndef BLUETOOTHSINK_H
#define BLUETOOTHSINK_H

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#include <functional>

#include "leds.h"
#include "userfeedbackhandler.h"

class BluetoothSink;

typedef std::function<void(BluetoothSink *sink, esp_a2d_connection_state_t state)> BluetoothSinkConnectCallback;

class BluetoothSink : public UserfeedbackHandler
{
private:
    I2SStream *out;
    BluetoothA2DPSink *sink;
    BluetoothSinkConnectCallback connectCallback;
    bool pauseMode;

public:
    BluetoothSink(I2SStream *output, BluetoothSinkConnectCallback connectCallback);

    void start(String bluetoothSSID);

    int pinCode();
    void confirmPinCode();

    virtual void toggleActiveState() override;
    virtual void previous() override;
    virtual void next() override;
    virtual bool volumeUp() override;
    virtual bool volumeDown() override;
};

#endif