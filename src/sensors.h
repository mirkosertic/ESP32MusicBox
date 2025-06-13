#ifndef SENSORS_H
#define SENSORS_H

#include "app.h"
#include "button.h"
#include "leds.h"

class Sensors
{
private:
    App *app;
    Leds *leds;
    Button *prev;
    Button *next;
    Button *startstop;

public:
    Sensors(App *app, Leds *leds);
    ~Sensors();

    void begin();

    void loop();

    bool isStartStopPressed();

    int getBatteryVoltage();    
};

#endif