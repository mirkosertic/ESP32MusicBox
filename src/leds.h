#ifndef LEDS_H
#define LEDS_H

#include <FastLED.h>

#include "app.h"

enum LEDState
{
    BOOT,
    PLAYER_STATUS,
    CARD_ERROR,
    CARD_DETECTED,
    VOLUME_CHANGE,
    BTCONNECTED
};

class Leds
{
private:
    App *app;
    LEDState state;
    CRGB leds[NUM_LEDS];
    long lastStateTime;
    long lastLoopTime;
    int framecounter;
    bool btspeakermode;

    void renderPlayerStatusIdle();
    void renderPlayerStatusPlaying();
    void renderCardError();
    void renderCardDetected();
    void renderVolumeChange();
    void renderBTConnected();

public:
    Leds(App *app, bool btspeakermode);

    void begin();

    void setState(LEDState state);

    void setBootProgress(int percent);

    void loop();
};

#endif