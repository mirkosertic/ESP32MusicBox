#ifndef BLUETOOTHSOURCE_H
#define BLUETOOTHSOURCE_H

#include <AudioTools.h>
#include <BluetoothA2DPSource.h>

class Leds;
class App;
class MediaPlayer;

class BluetoothSource
{
private:
    BufferRTOS<uint8_t> *buffer;
    QueueStream<uint8_t> *bluetoothout;
    I2SStream *out;
    BluetoothA2DPSource *source;
    Leds *leds;
    App *app;
    MediaPlayer *player;

public:
    BluetoothSource(I2SStream *output, Leds *leds, App *app, MediaPlayer *player);
    void start();

    int32_t readData(uint8_t *data, int32_t len);
    void handleAVRC(uint8_t key, bool isReleased);
};

#endif