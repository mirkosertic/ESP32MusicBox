#ifndef BLUETOOTHSINK_H
#define BLUETOOTHSINK_H

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "leds.h"

class App;

class BluetoothSink
{
private:
    I2SStream *out;
    BluetoothA2DPSink *sink;
    Leds *leds;
    App *app;

public:
    BluetoothSink(I2SStream *output, Leds *leds, App *app);

    void start();

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