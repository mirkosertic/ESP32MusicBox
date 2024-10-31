#include "leds.h"

#include <Arduino.h>

#include "pins.h"
#include "logging.h"

DEFINE_GRADIENT_PALETTE(volume_heatmap){
    224, 0, 255, 0,
    224, 255, 0, 0};

Leds::Leds(App *app)
{
    this->app = app;
    for (int i = 0; i < NUM_LEDS; i++)
    {
        this->leds[i] = CRGB::Black;
    }

    FastLED.addLeds<NEOPIXEL, NEOPIXEL_DATA>(leds, NUM_LEDS);

    this->lastLoopTime = 0;
    this->framecounter = 0;
}

void Leds::begin()
{
    FastLED.show();
}

void Leds::setState(LEDState state)
{
    INFO("Changing STATE");
    this->state = state;
    this->lastStateTime = millis();
}

void Leds::setBootProgress(int percent)
{
    this->state = BOOT;

    for (int i = 0; i < NUM_LEDS; i++)
    {
        this->leds[i] = CRGB::Black;
    }

    int progress = (int)(NUM_LEDS / 100.0 * min(percent, 100));
    for (int i = 0; i < progress; i++)
    {
        CHSV targetHSV = rgb2hsv_approximate(CRGB::White);
        targetHSV.v = 60;
        this->leds[i] = targetHSV;
    }
    FastLED.show();
}

void Leds::renderPlayerStatusIdle()
{
    CRGB color;
    if (this->app->isWifiConnected() || !this->app->isWifiEnabled())
    {
        color = CRGB::Green;
    }
    else
    {
        color = CRGB::Orange;
    }
    float minv = 40;
    float maxv = 80;
    int steps = 30;
    int framepos = this->framecounter % (steps * 2);
    int v = 0;
    if (framepos < steps)
    {
        v = (int)(minv + ((maxv - minv) / steps * framepos));
    }
    else
    {
        v = (int)(maxv - ((maxv - minv) / steps * (framepos - steps)));
    }
    CHSV targetHSV = rgb2hsv_approximate(color);
    targetHSV.v = v;
    for (int i = 0; i < NUM_LEDS; i++)
    {
        this->leds[i] = targetHSV;
    }
    FastLED.show();
}

void Leds::renderPlayerStatusPlaying()
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        this->leds[i] = CRGB::Black;
    }

    int maxbrightness = 80;

    int progressInPercent = this->app->playProgressInPercent();

    int total = NUM_LEDS * maxbrightness;
    int progress = (int)(total / 100.0 * progressInPercent);
    int index = 0;
    while (progress > 0)
    {
        int v = min(maxbrightness, progress);
        CHSV targetHSV = rgb2hsv_approximate(CRGB::Green);
        targetHSV.v = v;
        this->leds[index++] = targetHSV;
        progress -= v;
    }
    FastLED.show();
}

void Leds::renderCardError()
{
    long now = millis();
    if (now - this->lastStateTime > 1000)
    {
        // After one second inactivity / animation we jump back to the regular status indication
        this->state = PLAYER_STATUS;
        INFO("Switching state to PLAYER_STATUS");
    }
    else
    {
        // Red blinking
        int framepos = this->framecounter % 10;
        for (int i = 0; i < NUM_LEDS; i++)
        {
            this->leds[i] = CRGB::Black;
        }
        if (framepos > 5)
        {
            // All Status Red
            CHSV targetHSV = rgb2hsv_approximate(CRGB::Red);
            targetHSV.v = 80;
            for (int i = 0; i < NUM_LEDS; i++)
            {
                this->leds[i] = targetHSV;
            }
        }
        FastLED.show();
    }
}

void Leds::renderCardDetected()
{
    long now = millis();
    if (now - this->lastStateTime > 1000)
    {
        // After one second inactivity / animation we jump back to the regular status indication
        this->state = PLAYER_STATUS;
        INFO("Switching state to PLAYER_STATUS");
    }
    else
    {
        // Green blinking
        int framepos = this->framecounter % 10;
        for (int i = 0; i < NUM_LEDS; i++)
        {
            this->leds[i] = CRGB::Black;
        }
        if (framepos > 5)
        {
            // All Status Red
            CHSV targetHSV = rgb2hsv_approximate(CRGB::Green);
            targetHSV.v = 80;
            for (int i = 0; i < NUM_LEDS; i++)
            {
                this->leds[i] = targetHSV;
            }
        }
        FastLED.show();
    }
}

void Leds::renderVolumeChange()
{
    long now = millis();
    if (now - this->lastStateTime > 1000)
    {
        // After one second inactivity / animation we jump back to the regular status indication
        this->state = PLAYER_STATUS;
        INFO("Switching state to PLAYER_STATUS");
    }
    else
    {
        int volumePercent = (int)(this->app->getVolume() * 100);
        INFO_VAR("Volume is %d", volumePercent);

        for (int i = 0; i < NUM_LEDS; i++)
        {
            this->leds[i] = CRGB::Black;
        }

        int maxbrightness = 80;

        CRGBPalette16 myPalette = volume_heatmap;

        int total = NUM_LEDS * maxbrightness;
        int progress = (int)(total / 100.0 * volumePercent);
        int index = 0;
        while (progress > 0)
        {
            int v = min(maxbrightness, progress);
            CRGB color = ColorFromPalette(myPalette, (int) (255.0 * index / NUM_LEDS), v);
            //CHSV targetHSV = rgb2hsv_approximate(CRGB::Orange);
            //targetHSV.v = v;
            this->leds[index++] = color;
            progress -= v;
        }
        FastLED.show();
    }
}

void Leds::loop()
{
    long now = millis();
    if (this->lastLoopTime + 40 < now)
    {
        this->lastLoopTime = now;
        this->framecounter++;
        if (this->framecounter < 0)
        {
            this->framecounter = 0;
        }

        if (this->state == CARD_ERROR)
        {
            this->renderCardError();
        }
        else if (this->state == CARD_DETECTED)
        {
            this->renderCardDetected();
        }
        else if (this->state == VOLUME_CHANGE)
        {
            this->renderVolumeChange();
        }
        else if (this->state == PLAYER_STATUS)
        {
            if (this->app->isActive())
            {
                this->renderPlayerStatusPlaying();
            }
            else
            {
                this->renderPlayerStatusIdle();
            }
        }
    }
}