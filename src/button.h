#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

enum ButtonAction {
	PRESSED,
	PRESSED_FOR_LONG_TIME,
	RELEASED,
	RELEASED_AFTER_LONG_TIME
};

typedef std::function<void(ButtonAction)> ButtonHandler;

class Button {
private:
	int pin;
	long statetime;
	bool pressed;
	long sensitivity;
	ButtonHandler handler;

public:
	Button(int pin, long sensitivity, ButtonHandler handler);

	void loop();

	bool isPressed();
};

#endif
