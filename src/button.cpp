#include "button.h"

#include <Arduino.h>

#include "logging.h"

Button::Button(int pin, long sensitivity, ButtonHandler handler)
{
    this->pin = pin;
    this->pressed = false;
    this->statetime = 0;
    this->sensitivity = sensitivity;
    this->handler = handler;
    pinMode(pin, INPUT_PULLDOWN);
}

bool Button::isPressed()
{
    int input = digitalRead(pin);
    DEBUG_VAR("Button %d status %d", this->pin, input);
    return input == HIGH;
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
            this->pressed = true;

            this->handler(PRESSED);

            this->statetime = currenttime;
        }
    }

    if (this->pressed && (currenttime - this->statetime) > sensitivity)
    {
        this->handler(PRESSED_FOR_LONG_TIME);
    }
}
