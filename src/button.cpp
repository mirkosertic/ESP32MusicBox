#include "button.h"

#include <Arduino.h>

Button::Button(int pin, long sensitivity, ButtonHandler handler)
{
    this->pin = pin;
    this->pressed = false;
    this->statetime = 0;
    this->sensitivity = sensitivity;
    this->handler = handler;
    pinMode(pin, INPUT_PULLUP);
}

bool Button::isPressed()
{
    int input = digitalRead(pin);
    return input == LOW;
}

void Button::loop()
{
    long currenttime = millis();
    if (!this->isPressed())
    {
        // Open
        if (this->pressed)
        {
            if ((currenttime - this->statetime) > sensitivity)
            {
                this->handler(RELEASED_AFTER_LONG_TIME);
            }
            else
            {
                this->handler(RELEASED);
            }

            this->pressed = false;
            this->statetime = currenttime;
        }
    }
    else
    {
        // Close
        if (!this->pressed)
        {
            this->handler(PRESSED);

            this->pressed = true;
            this->statetime = currenttime;
        }
    }

    if (this->pressed && (currenttime - this->statetime) > sensitivity)
    {
        this->handler(PRESSED_FOR_LONG_TIME);
    }
}
