#include "sensors.h"

#include "logging.h"
#include "pins.h"

void sensorsdelegate(void *arguments) {
	INFO("Sensors task started");
	Sensors *sensors = (Sensors *) arguments;
	while (true) {
		sensors->loop();
		delay(15);
	}
}

Sensors::Sensors(Leds *leds) {
	this->leds = leds;

	pinMode(GPIO_VOLTAGE_MEASURE, INPUT);

	this->startstop = new Button(GPIO_STARTSTOP, 300, [this](ButtonAction action) {
        if (action == PRESSED)
        {
            INFO("Start / Stop button pressed");
            this->handler->toggleActiveState();
        } });

	this->prev = new Button(GPIO_PREVIOUS, 300, [this](ButtonAction action) {
        if (action == RELEASED) 
        {
            INFO("Prev button released");            
            this->handler->previous();
        }
        if (action == PRESSED_FOR_LONG_TIME)
        {   
            if (this->handler->volumeDown())
            {
                INFO("Decrementing volume");
                this->leds->setState(VOLUME_CHANGE);
            }
        } });

	this->next = new Button(GPIO_NEXT, 300, [this](ButtonAction action) {
        if (action == RELEASED)
        {
            INFO("Next button released");                        
            this->handler->next();
        }
        if (action == PRESSED_FOR_LONG_TIME)
        {
            if (this->handler->volumeUp())
            {
                INFO("Incrementing volume");
                this->leds->setState(VOLUME_CHANGE);
            }
        } });
}

void Sensors::begin(UserfeedbackHandler *handler) {
	this->handler = handler;

	// Start the loop as a separate task running in the background
	xTaskCreate(sensorsdelegate, "Sensors", 8192, this, 2, NULL);
}

Sensors::~Sensors() {
	delete this->prev;
	delete this->next;
	delete this->startstop;
}

void Sensors::loop() {
	this->prev->loop();
	this->next->loop();
	this->startstop->loop();
}

bool Sensors::isStartStopPressed() {
	return this->startstop->isPressed();
}

bool Sensors::isPreviousPressed() {
	return this->prev->isPressed();
}

bool Sensors::isNextPressed() {
	return this->next->isPressed();
}

int Sensors::getBatteryVoltage() {
	int value = analogRead(GPIO_VOLTAGE_MEASURE);
	INFO("Battery voltage ADC is %d", value);
	return (int) (value / 4096.0 * 7.445 * 1000.0);
}
