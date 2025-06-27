#ifndef BOOMBOXMODE_H
#define BOOMBOXMODE_H

#include "mode.h"

class BoomboxMode : public Mode 
{
    public:
        BoomboxMode(Led *led, Sensors *sensors);
};

#endif