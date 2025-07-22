#include "sensors.h"

#include "logging.h"
#include "pins.h"

Sensors::Sensors() {
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
            }
        } });
}

void Sensors::begin(UserfeedbackHandler *handler) {
	this->handler = handler;
}

Sensors::~Sensors() {
	delete this->prev;
	delete this->next;
	delete this->startstop;
}

void Sensors::loop() {
	static long lastcheck = millis();

	// We check the buttons not in every main loop iteration, we check them timed...
	long now = millis();
	if (now - lastcheck > 30) {
		lastcheck = now;
		this->prev->loop();
		this->next->loop();
		this->startstop->loop();
	}
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
