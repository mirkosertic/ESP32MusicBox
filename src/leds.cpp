#include "leds.h"

#include "pins.h"

Leds::Leds()
{
    this->leds[0] = CRGB::Black;
    this->leds[1] = CRGB::Black;
    this->leds[2] = CRGB::Black;
    this->leds[3] = CRGB::Black;
    this->leds[4] = CRGB::Black;
    this->leds[5] = CRGB::Black;
    this->leds[6] = CRGB::Black;
    this->leds[7] = CRGB::Black;

    FastLED.addLeds<NEOPIXEL, NEOPIXEL_DATA>(leds, 8);
}

void Leds::begin()
{
    FastLED.show();
}

void Leds::setInitState(int level)
{
    for (int i = 0; i < level; i++)
    {
        CHSV targetHSV = rgb2hsv_approximate(CRGB::White);
        targetHSV.v = 80;
        this->leds[i] = targetHSV;
    }
    FastLED.show();
}