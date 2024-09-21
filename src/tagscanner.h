#ifndef TAGSCANNER_H
#define TAGSCANNER_H

#include <Adafruit_PN532.h>
#include <Wire.h>

// Magic number (2 bytes)
#define MAGIC_NUMBER 0xA55A

// Data structure (48 bytes total)
struct TagData
{
    uint16_t magic;    // 2 bytes
    uint8_t data[44];  // 44 bytes for user data
    uint16_t checksum; // 2 bytes

    TagData();
    TagData(const TagData &source);
};

typedef std::function<void(bool, bool, uint8_t *, String, uint8_t, String, TagData)> TagDetectedCallback;
typedef std::function<void()> NoTagCallback;

class TagScanner
{
private:
    TwoWire *wire;
    Adafruit_PN532 *pn532;

    TagDetectedCallback tagDetectedCallback;
    NoTagCallback noTagCallback;

    bool tagPresent;
    String currentTagName;

    QueueHandle_t dataToWrite;

public:
    TagScanner(TwoWire *wire, uint8_t irq, uint8_t reset);

    ~TagScanner();

    void scan();

    void write(uint8_t *userdata, uint8_t size);

    void begin(TagDetectedCallback callback, NoTagCallback noTagCallback);

    void clearTag();
};

#endif