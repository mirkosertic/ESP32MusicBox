#ifndef I2CDEBUG_H
#define I2CDEBUG_H

#include <Wire.h>

class I2CDebug {
private:
	TwoWire *wire;

public:
	I2CDebug(TwoWire *wire);

	void printDevices();
};

#endif
