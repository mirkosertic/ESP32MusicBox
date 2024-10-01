#include "buttons.h"

#include "logging.h"
#include "pins.h"

void buttonsdelegate(void *arguments)
{
    INFO("Buttons task started");
    Buttons *buttons = (Buttons *)arguments;
    while (true)
    {
        buttons->loop();
        delay(15);
    }
}

Buttons::Buttons(App *app)
{
    this->app = app;
    this->startstop = new Button(GPIO_STARTSTOP, 300, [this](ButtonAction action)
                                 {
        if (action == PRESSED)
        {
           this->app->toggleActiveState();
        } });

    this->prev = new Button(GPIO_PREVIOUS, 300, [this](ButtonAction action)
                            {
        if (action == RELEASED) 
        {
            this->app->previous();
        }
        if (action == PRESSED_FOR_LONG_TIME)
        {   
            float volume = this->app->getVolume();
            if (volume >= 0.02) 
            {
                INFO("Decrementing volume");
                this->app->setVolume(volume - 0.02);
                //kit.setVolume(volume - 0.02);        
            } else{
                INFO("Minimum volume reached");
            }
        } });

    this->next = new Button(GPIO_NEXT, 300, [this](ButtonAction action)
                            {
        if (action == RELEASED)
        {
            this->app->next();
        }
        if (action == PRESSED_FOR_LONG_TIME)
        {
            float volume = this->app->getVolume();
            if (volume <= 0.98) 
            {
                INFO("Incrementing volume");
                this->app->setVolume(volume + 0.02);
                //kit.setVolume(volume + 0.02);
            } else {
                INFO("Maximum volume reached");
            }
        } });
}

Buttons::~Buttons()
{
    delete this->prev;
    delete this->next;
    delete this->startstop;
}

void Buttons::begin()
{
    // Start the loop as a separate task running in the background
    xTaskCreate(buttonsdelegate, "Buttons", 4096, this, 5, NULL);
}

void Buttons::loop()
{
    this->prev->loop();
    this->next->loop();
    this->startstop->loop();
}

bool Buttons::isStartStopPressed()
{
    return this->startstop->isPressed();
}