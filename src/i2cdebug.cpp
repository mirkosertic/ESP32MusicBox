#include "i2cdebug.h"
#include "logging.h"

I2CDebug::I2CDebug(TwoWire *wire)
{
    this->wire = wire;
}

void I2CDebug::printDevices()
{
    int nDevices;

    INFO("Scanning I2C devices");

    nDevices = 0;
    for (byte address = 1; address < 127; address++)
    {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        this->wire->beginTransmission(address);
        byte error = this->wire->endTransmission();

        if (error == 0)
        {
            INFO("Found device at 0x%02X", address);

            nDevices++;
        }
        else if (error == 4)
        {
            INFO("Unknown error at 0x%02X", address);
        }
    }

    INFO("Scan done, %d devices found", nDevices);
}
