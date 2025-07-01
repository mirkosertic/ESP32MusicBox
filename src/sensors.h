#ifndef SENSORS_H
#define SENSORS_H

#include "button.h"
#include "leds.h"
#include "userfeedbackhandler.h"

class Sensors {
private:
	UserfeedbackHandler *handler;
	Button *prev;
	Button *next;
	Button *startstop;

public:
	Sensors();
	~Sensors();

	void begin(UserfeedbackHandler *handler);

	void loop();

	bool isStartStopPressed();

	bool isPreviousPressed();

	bool isNextPressed();

	int getBatteryVoltage();
};

#endif
