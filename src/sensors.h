#ifndef SENSORS_H
#define SENSORS_H

#include "userfeedbackhandler.h"
#include "button.h"
#include "leds.h"

class Sensors
{
private:
    UserfeedbackHandler *handler;
    Leds *leds;
    Button *prev;
    Button *next;
    Button *startstop;

public:
    Sensors(Leds *leds);
    ~Sensors();

    void begin(UserfeedbackHandler *handler);

    void loop();

    bool isStartStopPressed();

    bool isPreviousPressed();

    bool isNextPressed();

    int getBatteryVoltage();
};

#endif