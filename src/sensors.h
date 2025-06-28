#ifndef SENSORS_H
#define SENSORS_H

#include "button.h"
#include "leds.h"
#include "userfeedbackhandler.h"

class Sensors {
private:
	UserfeedbackHandler *handler;
	Leds *leds;
	Button *prev;
	Button *next;
	Button *startstop;

public:
	Sensors(Leds *leds);
	~Sensors();

	void begin(UserfeedbackHandler *handler);

	void loop();

	bool isStartStopPressed();

	bool isPreviousPressed();

	bool isNextPressed();

	int getBatteryVoltage();
};

#endif
