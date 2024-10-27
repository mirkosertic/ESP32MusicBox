#ifndef LEDS_H
#define LEDS_H

#include <FastLED.h>

class Leds
{
private:
    CRGB leds[8];

public:
    Leds();

    void begin();

    void setInitState(int level);
};

#endif