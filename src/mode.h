#ifndef MODE_H
#define MODE_H

#include "led.h"
#include "sensors.h"

class Mode 
{
    protected:
        Led *led;
        Sensors *sensors;

    public:
        Mode(Led *led, Sensors *sensors)
        {
            this->led = led;
            this->sensors = sensors;  
        }

        virtual void setup() = 0;
        virtual void run() = 0;
};

#endif