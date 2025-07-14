#ifndef MODE_H
#define MODE_H

#include "settings.h"

#include "leds.h"
#include "sensors.h"

#include <AudioTools.h>

typedef enum {
	MODE_IDLE,
	MODE_NOT_IDLE
} ModeStatus;

class Mode {
private:
	void i2cinit();
	void i2sinit();
	void sdinit();

protected:
	Leds *leds;
	Sensors *sensors;
	AudioInfo defaultAudioInfo;
	I2SStream *i2sstream;
	Settings *settings;

public:
	Mode(Leds *leds, Sensors *sensors);

	virtual void setup();
	virtual ModeStatus loop();
	virtual void shutdown() = 0;
};

#endif
