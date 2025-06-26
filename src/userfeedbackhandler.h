#ifndef USERFEEDBACKHANDLER_H
#define USERFEEDBACKHANDLER_H

#include <Arduino.h>

class UserfeedbackHandler
{
public:
    virtual bool volumeUp() = 0;

    virtual bool volumeDown() = 0;

    virtual void toggleActiveState() = 0;

    virtual void previous() = 0;

    virtual void next() = 0;
};

#endif