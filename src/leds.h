#ifndef LEDS_H
#define LEDS_H

#include <FastLED.h>

#include "app.h"

#define NUM_LEDS 24

enum LEDState
{
    BOOT,
    PLAYER_STATUS,
    CARD_ERROR,
    CARD_DETECTED,
    VOLUME_CHANGE
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

    void renderPlayerStatusIdle();
    void renderPlayerStatusPlaying();
    void renderCardError();
    void renderCardDetected();
    void renderVolumeChange();

public:
    Leds(App *app);

    void begin();

    void setState(LEDState state);

    void setBootProgress(int percent);

    void loop();
};

#endif