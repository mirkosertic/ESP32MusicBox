#ifndef BOOMBOXMODE_H
#define BOOMBOXMODE_H

#include "bluetoothsink.h"
#include "mode.h"

class BoomboxMode : public Mode {
protected:
	BluetoothSink *bluetoothsink;
	bool connected;

public:
	BoomboxMode(Leds *leds, Sensors *sensors);

	void setup() override;
	virtual ModeStatus loop() override;
	virtual void shutdown() override;
};

#endif
