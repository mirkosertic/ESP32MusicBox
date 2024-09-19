#include "tagscanner.h"

#include "logging.h"

String uint8ArrayToHexString(uint8_t *array, size_t length)
{
    String result = "";
    for (size_t i = 0; i < length; i++)
    {
        if (array[i] < 0x10)
        {
            result += "0";
        }
        result += String(array[i], HEX);
    }
    return result;
}

// Function to compute XOR-based checksum
uint16_t computeChecksum(const TagData &packet)
{
    uint16_t xorChecksum = MAGIC_NUMBER;
    for (int i = 0; i < sizeof(packet.data); i++)
    {
        xorChecksum ^= (packet.data[i] << (8 * (i % 2)));
    }
    return xorChecksum;
}

// Function to validate the data structure
bool validateDataPacket(const TagData &packet)
{
    // Check magic number
    if (packet.magic != MAGIC_NUMBER)
    {
        return false;
    }

    // Compute and compare checksum
    return (computeChecksum(packet) == packet.checksum);
}

// Function to initialize the data structure and compute checksum
void initializeDataPacket(TagData &packet, const uint8_t *userData, size_t dataSize)
{
    // Set magic number
    packet.magic = MAGIC_NUMBER;

    // Copy user data
    size_t copySize = min(dataSize, sizeof(packet.data));
    memcpy(packet.data, userData, copySize);

    // Fill remaining data with zeros if necessary
    if (copySize < sizeof(packet.data))
    {
        memset(packet.data + copySize, 0, sizeof(packet.data) - copySize);
    }

    // Compute checksum
    packet.checksum = computeChecksum(packet);
}

void scandispatcher(void *parameters)
{
    TagScanner *target = (TagScanner *)parameters;
    target->scan();
}

TagData::TagData()
{
    this->checksum = 0;
    this->magic = 0;
    memset(&this->data[0], 0, 44);
}

TagData::TagData(const TagData &source)
    : TagData()
{
    this->magic = source.magic;
    memcpy(&this->data[0], &source.data[0], 44);
    this->checksum = source.checksum;
}

TagScanner::TagScanner(TwoWire *wire, uint8_t irq, uint8_t reset)
{
    this->wire = wire;
    this->pn532 = new Adafruit_PN532(irq, reset, &Wire1);
    this->tagPresent = false;
    this->currentTagName = "";
}

TagScanner::~TagScanner()
{
    delete this->pn532;
}

void TagScanner::scan()
{
    uint8_t tagscanneruid[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t tagscanneruidLength;

    while (true)
    {
        // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
        // 'uid' will be populated with the UID, and uidLength will indicate
        // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
        bool tagscannersuccess = this->pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, &tagscanneruid[0], &tagscanneruidLength, 200);

        if (tagscannersuccess)
        {

            String uidStr = uint8ArrayToHexString(&tagscanneruid[0], tagscanneruidLength);

            // Display some basic information about the card
            INFO("Found an ISO14443A tag");
            // INFO_VAR("UID Length: %d bytes, UID is %s", (int) tagscanneruidLength, uidStr.c_str());

            if (tagscanneruidLength == 4)
            {
                // We probably have a Mifare Classic card ...
                INFO("Seems to be a Mifare Classic tag (4 byte UID)");

                String tagscannertagname = "Mifare Classic " + uidStr;

                uint8_t datatowrite[48];
                bool somethingtowrite = false;
                {
                    std::lock_guard<std::mutex> lck(this->rwmutex);
                    if (this->datatowrite.size() > 0)
                    {
                        INFO("Data to write is pending");

                        this->pn532->PrintHexChar(&datatowrite[0], 16);
                        this->pn532->PrintHexChar(&datatowrite[16], 16);
                        this->pn532->PrintHexChar(&datatowrite[32], 16);

                        somethingtowrite = true;
                        TagData tagdata = this->datatowrite[0];

                        memcpy(&datatowrite[0], &tagdata, 48);

                        this->datatowrite.clear();
                    }
                }

                if (!this->tagPresent || this->currentTagName != tagscannertagname || somethingtowrite)
                {
                    TagData tagdata;

                    // Now we need to try to authenticate it for read/write access
                    // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
                    uint8_t keya[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

                    // Start with block 4 (the first block of sector 1) since sector 0
                    // contains the manufacturer data and it's probably better just
                    // to leave it alone unless you know what you're doing
                    uint8_t data[48];
                    bool authenticated = this->pn532->mifareclassic_AuthenticateBlock(&tagscanneruid[0], tagscanneruidLength, 4, 0, keya);
                    if (authenticated)
                    {
                        this->tagPresent = true;
                        this->currentTagName = tagscannertagname;

                        INFO("Sector 1 (Blocks 4..7) has been authenticated");

                        if (somethingtowrite)
                        {
                            INFO("Writing data to the tag");
                            bool writestatusblock4 = this->pn532->mifareclassic_WriteDataBlock(4, &datatowrite[0]);
                            bool writestatusblock5 = this->pn532->mifareclassic_WriteDataBlock(5, &datatowrite[16]);
                            bool writestatusblock6 = this->pn532->mifareclassic_WriteDataBlock(6, &datatowrite[32]);
                            if (!writestatusblock4 || !writestatusblock5 || !writestatusblock6)
                            {
                                WARN("Error writing some blocks to the tag.");
                                continue;
                            }
                            INFO("Data written");

                            this->noTagCallback();
                        }

                        bool block4status = this->pn532->mifareclassic_ReadDataBlock(4, &data[0]);
                        bool block5status = this->pn532->mifareclassic_ReadDataBlock(5, &data[16]);
                        bool block6status = this->pn532->mifareclassic_ReadDataBlock(6, &data[32]);

                        if (block4status && block5status && block6status)
                        {
                            INFO("Blocks 4-7 read");

                            this->pn532->PrintHexChar(&data[0], 16);
                            this->pn532->PrintHexChar(&data[16], 16);
                            this->pn532->PrintHexChar(&data[32], 16);

                            memcpy(&tagdata, &data[0], 48);

                            if (validateDataPacket(tagdata))
                            {
                                INFO("Known tag detected");
                                this->tagDetectedCallback(true, true, &tagscanneruid[0], uidStr, tagscanneruidLength, tagscannertagname, tagdata);
                            }
                            else
                            {
                                WARN("Unknown tag detected");
                                this->tagDetectedCallback(true, false, &tagscanneruid[0], uidStr, tagscanneruidLength, tagscannertagname, tagdata);
                            }
                        }
                        else
                        {
                            WARN("Could not read all data blocks");
                            this->tagDetectedCallback(false, false, &tagscanneruid[0], uidStr, tagscanneruidLength, tagscannertagname, tagdata);
                        }
                    }
                    else
                    {
                        WARN("Card authentication error");
                        this->tagDetectedCallback(false, false, &tagscanneruid[0], uidStr, tagscanneruidLength, tagscannertagname, tagdata);
                    }
                }
            }

            if (tagscanneruidLength == 7)
            {
                // We probably have a Mifare Ultralight card ...
                INFO("Seems to be a Mifare Ultralight tag (7 byte UID)");

                String tagscannertagname = "Mifare Ultralight " + uidStr;

                if (!this->tagPresent || this->currentTagName != tagscannertagname)
                {
                    this->tagPresent = true;
                    this->currentTagName = tagscannertagname;

                    TagData tagdata;

                    // Try to read the first general-purpose user page (#4)
                    uint8_t data[48];
                    bool page4result = this->pn532->mifareultralight_ReadPage(4, &data[0]);
                    bool page5result = this->pn532->mifareultralight_ReadPage(5, &data[4]);
                    bool page6result = this->pn532->mifareultralight_ReadPage(6, &data[8]);
                    bool page7result = this->pn532->mifareultralight_ReadPage(7, &data[12]);
                    bool page8result = this->pn532->mifareultralight_ReadPage(8, &data[16]);
                    bool page9result = this->pn532->mifareultralight_ReadPage(9, &data[20]);
                    bool page10result = this->pn532->mifareultralight_ReadPage(10, &data[24]);
                    bool page11esult = this->pn532->mifareultralight_ReadPage(11, &data[28]);
                    bool page12result = this->pn532->mifareultralight_ReadPage(12, &data[32]);
                    bool page13result = this->pn532->mifareultralight_ReadPage(13, &data[36]);
                    bool page14result = this->pn532->mifareultralight_ReadPage(14, &data[40]);
                    bool page15result = this->pn532->mifareultralight_ReadPage(15, &data[44]);

                    if (page4result && page5result && page6result && page7result && page8result && page9result && page10result && page11esult && page12result && page13result && page14result && page15result)
                    {
                        INFO("Pages 4-15 read");

                        this->pn532->PrintHexChar(&data[0], 16);
                        this->pn532->PrintHexChar(&data[16], 16);
                        this->pn532->PrintHexChar(&data[32], 16);

                        memccpy(&tagdata, &data[0], 1, 48);

                        if (validateDataPacket(tagdata))
                        {
                            INFO("Known tag detected");
                            this->tagDetectedCallback(true, true, &tagscanneruid[0], uidStr, tagscanneruidLength, tagscannertagname, tagdata);
                        }
                        else
                        {
                            WARN("Unknown tag detected");
                            this->tagDetectedCallback(true, false, &tagscanneruid[0], uidStr, tagscanneruidLength, tagscannertagname, tagdata);
                        }
                    }
                    else
                    {
                        WARN("Could not read all data pages");
                        this->tagDetectedCallback(false, false, &tagscanneruid[0], uidStr, tagscanneruidLength, tagscannertagname, tagdata);
                    }
                }
            }
        }
        else
        {
            if (this->tagPresent)
            {
                INFO("No tag detected");
                this->noTagCallback();
                this->tagPresent = false;
            }
        }
        delay(500);
    }
}

void TagScanner::begin(TagDetectedCallback callback, NoTagCallback noTagCallback)
{
    this->tagDetectedCallback = callback;
    this->noTagCallback = noTagCallback;
    unsigned long versiondata = this->pn532->getFirmwareVersion();
    if (!versiondata)
    {
        WARN("Could not find a board!");
        while (true)
            ;
    }
    INFO_VAR("Chip PN5 is %d", (versiondata >> 24) & 0xFF);
    INFO_VAR("Firmware ver. %d.%d", (versiondata >> 16) & 0xFF, (versiondata >> 8) & 0xFF);
    this->pn532->SAMConfig();

    xTaskCreate(scandispatcher, "NFC Scanner", 2048, this, 20, NULL);
}

void TagScanner::write(uint8_t *userdata, uint8_t size)
{
    std::lock_guard<std::mutex> lck(this->rwmutex);

    TagData tagdata;
    initializeDataPacket(tagdata, userdata, size);

    this->datatowrite.clear();
    this->datatowrite.push_back(tagdata);
}

void TagScanner::clearTag()
{
    std::lock_guard<std::mutex> lck(this->rwmutex);

    TagData tagdata;

    this->datatowrite.clear();
    this->datatowrite.push_back(tagdata);
}
