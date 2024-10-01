#ifndef BUTTONS_H
#define BUTTONS_H

#include "app.h"
#include "button.h"

class Buttons
{
private:
    App *app;
    Button *prev;
    Button *next;
    Button *startstop;

public:
    Buttons(App *app);
    ~Buttons();

    void begin();

    void loop();

    bool isStartStopPressed();
};

#endif