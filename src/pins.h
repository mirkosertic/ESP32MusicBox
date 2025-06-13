// Make sure the the buttons are assigned to IO pins with pullup/puppdown resistors configurable by software!
#define GPIO_STARTSTOP GPIO_NUM_4
#define GPIO_PREVIOUS GPIO_NUM_16
#define GPIO_NEXT GPIO_NUM_17

#define GPIO_WIRE_SDA GPIO_NUM_21
#define GPIO_WIRE_SCL GPIO_NUM_22

#define GPIO_PN532_IRQ GPIO_NUM_32
#define GPIO_PN532_RST GPIO_NUM_33

#define GPIO_NEOPIXEL_DATA GPIO_NUM_25

#define GPIO_I2S_BCK GPIO_NUM_26
#define GPIO_I2S_WS GPIO_NUM_14
#define GPIO_I2S_DATA GPIO_NUM_27

#define GPIO_SPI_SS GPIO_NUM_5
#define GPIO_SPI_SCK GPIO_NUM_18
#define GPIO_SPI_MOSI GPIO_NUM_23
#define GPIO_SPI_MISO GPIO_NUM_19

// analogRead(GPIO_VOLTAGE_MEASURE) / 4096.0 * 7.445
#define GPIO_VOLTAGE_MEASURE GPIO_NUM_35