#ifndef BUTTONS_H
#define BUTTONS_H

#include "app.h"
#include "button.h"
#include "leds.h"

class Buttons
{
private:
    App *app;
    Leds *leds;
    Button *prev;
    Button *next;
    Button *startstop;

public:
    Buttons(App *app, Leds *leds);
    ~Buttons();

    void begin();

    void loop();

    bool isStartStopPressed();
};

#endif