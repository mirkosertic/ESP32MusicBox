#ifndef RFIDPLAYERMODE_H
#define RFIDPLAYERMODE_H

#include "mode.h"

class RfidPlayerMode : public Mode 
{
    public:
        RfidPlayerMode(Led *led, Sensors *sensors);

        void setup() override;
        void run() override;        
};

#endif