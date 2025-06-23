#include <WiFi.h>
#include <Wire.h>
#include <SD.h>
#include <SPIFFS.h>

#include <esp_task_wdt.h>

#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <ArduinoJson.h>
#include <Driver.h>
#include <esp_idf_version.h>
#include <esp_arduino_version.h>

// #include <DNSServer.h>

#include "bluetoothsource.h"
#include "bluetoothsink.h"
#include "i2cdebug.h"
#include "settings.h"
#include "tagscanner.h"
#include "app.h"
#include "mqtt.h"
#include "frontend.h"
#include "mediaplayer.h"
#include "mediaplayersource.h"
#include "commands.h"
#include "logging.h"
#include "voiceassistant.h"
#include "pins.h"
#include "leds.h"
#include "sensors.h"

const char *CONFIGURATION_FILE = "/configuration.json";
const char *STARTFILEPATH = "/";
const char *MP3_FILE = ".mp3";
const int HTTP_SERVER_PORT = 80;

// IPAddress apIP(192, 168, 4, 1);
// DNSServer dnsServer;

MediaPlayerSource *source;
I2SStream *i2s;
MP3DecoderHelix *decoder;
MediaPlayer *player;

BluetoothSource *bluetoothsource;
BluetoothSink *bluetoothsink;
Settings *settings;

TagScanner *tagscanner;
App *app;
Leds *leds;
Sensors *sensors = NULL;

// Only relevant in case of WiFi enabled
WiFiClient *wifiClient = NULL;
Frontend *frontend = NULL;
VoiceAssistant *assistant = NULL;
MQTT *mqtt = NULL;

QueueHandle_t commandsHandle;

long startupTime = millis();

void wifiscannertask(void *arguments)
{
  INFO("WiFi scanner task started");
  while (true)
  {
    settings->rescanForBetterNetworksAndReconfigure();
    delay(60000);
  }
}

void callbackPrintMetaData(MetaDataType type, const char *str, int len)
{
  INFO("Detected Metadata %s : %s", toStr(type), str);
}

void setup()
{
  Serial.begin(115200);

  INFO("Setup started");
  INFO("Running on Arduino : %d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  INFO("Running on ESP IDF : %s", esp_get_idf_version());
  INFO("Max  HEAP  is %d", ESP.getHeapSize());
  INFO("Free HEAP  is %d", ESP.getFreeHeap());
  INFO("Max  PSRAM is %d", ESP.getPsramSize());
  INFO("Free PSRAM is %d", ESP.getFreePsram());

  INFO("I2C connection init");
  if (!Wire1.begin(GPIO_WIRE_SDA, GPIO_WIRE_SCL, 100000))
  {
    WARN("I2C initialization failed!");
    while (true)
      ;
  }

  // Some debug output
  I2CDebug debug(&Wire1);
  debug.printDevices();

  INFO("Creating core components")
  i2s = new I2SStream();

  source = new MediaPlayerSource(STARTFILEPATH, MP3_FILE, true);
  decoder = new MP3DecoderHelix();

  INFO("Using I2S for default sound output");
  player = new MediaPlayer(*source, *i2s, *decoder);

  // Inform the player what to output
  AudioInfo info(44100, 2, 16);
  player->setAudioInfo(info);

  settings = new Settings(&SD, CONFIGURATION_FILE);

  tagscanner = new TagScanner(&Wire1, GPIO_PN532_IRQ, GPIO_PN532_RST);
  app = new App(tagscanner, source, player, settings, player);
  leds = new Leds();
  sensors = new Sensors(app, leds);
  INFO("Core components created. Free HEAP is %d", ESP.getFreeHeap());

  INFO("LED Status display init");
  leds->begin();

  INFO("Free HEAP is %d", ESP.getFreeHeap());

  commandsHandle = xQueueCreate(10, sizeof(CommandData));
  if (commandsHandle == NULL)
  {
    WARN("Command queue could not be created. Halt.");
    while (true)
      ;
  }

  // esp_task_wdt_init(30, true); // 30 Sekunden Timeout
  // esp_task_wdt_add(NULL);

#if USE_AUDIO_LOGGING
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
#endif

  app->setDeviceType("ESP32 Musikbox");
  app->setName(DEVICENAME);
  app->setManufacturer("Mirko Sertic");
  app->setVersion("v1.0");
  app->setServerPort(HTTP_SERVER_PORT);

  leds->setState(BOOT);
  leds->setBootProgress(0);

  // setup output
  auto cfg = i2s->defaultConfig(TX_MODE);
  cfg.pin_bck = GPIO_I2S_BCK;
  cfg.pin_ws = GPIO_I2S_WS;
  cfg.pin_data = GPIO_I2S_DATA;
  cfg.i2s_format = I2S_STD_FORMAT; // default format
  cfg.copyFrom(info);

  INFO("I2S sound system init");
  if (!i2s->begin(cfg))
  {
    WARN("Could not start I2S sound system!");
    while (true)
      ;
  }
  leds->setBootProgress(10);

  // Setup SD-Card
  INFO("SD-Card init");
  SPI.begin(GPIO_SPI_SCK, GPIO_SPI_MISO, GPIO_SPI_MOSI, GPIO_SPI_SS);
  // SPIClass spi = SPIClass(VSPI);
  // spi.begin(GPIO_SPI_SCK, GPIO_SPI_MISO, GPIO_SPI_MOSI, GPIO_SPI_SS);

  // if (!SD.begin(GPIO_SPI_SS, spi))
  if (!SD.begin(GPIO_SPI_SS))
  {
    WARN("Could not enable SD-Card over SPI!");
    while (true)
      ;
  }
  leds->setBootProgress(20);

  // AT THIS POINT THE SD CARD IS PROPERLY CONFIGURED
  source->begin();

  leds->setBootProgress(30);

  leds->setBootProgress(40);

  if (!sensors->isPreviousPressed())
  {
    INFO("Retrieving system configuration");
    if (!settings->readFromConfig())
    {
      WARN("Could not read configuration file from SDCard!");
      while (true)
        ;
    }
    else
    {
      if (sensors->isStartStopPressed())
      {
        settings->resetStoredBSSID();
      }
      settings->initializeWifiFromSettings();
    }

    INFO("WiFi configuration and creating networking components. Free HEAP is %d", ESP.getFreeHeap());
    wifiClient = new WiFiClient();
    frontend = new Frontend(&SD, app, HTTP_SERVER_PORT, MP3_FILE, settings);
    if (settings->isVoiceAssistantEnabled())
    {
      INFO("Initializing voice assistant client. Free HEAP is %d", ESP.getFreeHeap());
      assistant = new VoiceAssistant(i2s, settings);
    }
    mqtt = new MQTT(*wifiClient, app);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(app->computeTechnicalName().c_str());
    WiFi.setAutoReconnect(true);

    INFO("Bluetooth source configuration. Free HEAP is %d", ESP.getFreeHeap());
    bluetoothsource = new BluetoothSource(i2s, leds, app, player);
    bluetoothsource->start();

    INFO("Bluetooth source initialized. Free HEAP is %d", ESP.getFreeHeap());
  }
  else
  {
    INFO("Retrieving system configuration");
    if (!settings->readFromConfig())
    {
      WARN("Could not read configuration file from SDCard!");
      while (true)
        ;
    }

    INFO("Bluetooth sink configuration. Free HEAP is %d", ESP.getFreeHeap());
    bluetoothsink = new BluetoothSink(i2s, leds, app);
    bluetoothsink->start();

    INFO("Bluetooth sink initialized. Free HEAP is %d", ESP.getFreeHeap());
  }

  // WiFi.softAP(app->computeTechnicalName().c_str());

  // dnsServer.start(53, "*", apIP);

  leds->setBootProgress(50);

  player->setMetadataCallback(callbackPrintMetaData);
  decoder->driver()->setInfoCallback([](MP3FrameInfo &info, void *ref)
                                     { INFO("Got libHelix Info %d bitrate %d bits per sample %d channels", info.bitrate, info.bitsPerSample, info.nChans); }, nullptr);

  leds->setBootProgress(60);

  if (!app->isActAsBluetoothSpeaker())
  {
    INFO("NFC reader init");
    tagscanner->begin([](bool authenticated, bool knownTag, uint8_t *uid, String uidStr, uint8_t uidlength, String tagName, TagData tagdata)
                      {
      if (authenticated) 
      {
        // A tag was detected
        if (mqtt != NULL) {
          mqtt->publishTagScannerInfo(tagName);
        }

        app->setTagData(knownTag, tagName, uid, uidlength, tagdata);

        if (knownTag) {
          Serial.println("(tag) Tag debug data");
          for (int i = 0; i < 44; i++) {
              if (i == 16 || i == 32) {
                  Serial.println();
              }
              uint8_t b = tagdata.data[i];
              if (b < 16) {
                  Serial.print("0");
              }
              Serial.print(b, HEX);
              Serial.print(" ");
          }
          Serial.println();

          CommandData command;
          memcpy(&command, &tagdata.data[0], 44);

          // Pass the command to the command queue
          int ret = xQueueSend(commandsHandle, (void *)&command, 0);
          if (ret == pdTRUE) {
            // No problem here  
          } else if (ret == errQUEUE_FULL) {
            WARN("Unable to send data into the command queue");
          } 

          JsonDocument result;
          result["UID"] = uidStr;

          String target;
          serializeJson(result, target);

          if (mqtt != NULL) {
            mqtt->publishScannedTag(target);
          }

          leds->setState(CARD_DETECTED);
        }
        else
        {
          leds->setState(CARD_ERROR);
        }
      }
      else {
        // Authentication error
        if (mqtt != NULL) {
          mqtt->publishTagScannerInfo("Authentication error");
        }

        app->noTagPresent();

        leds->setState(CARD_ERROR);
      } }, []()
                      {
      // No tag currently detected
      if (mqtt != NULL)  {
        mqtt->publishTagScannerInfo("");
      }

      app->noTagPresent(); });

    leds->setBootProgress(70);
    INFO("NFC reader init finished");
  }

  source->setChangeIndexCallback([](Stream *next)
                                 { 
                                  INFO("In info callback");
                                  if (next != nullptr)
                                  {
                                    const char *songinfo = source->toStr();
                                    if (songinfo && mqtt != NULL)
                                    {
                                      mqtt->publishCurrentSong(String(songinfo));
                                    }

                                    app->incrementStateVersion();
                                  }
                                  INFO("Done"); });

  // Init file browser
  strcpy(app->getCurrentPath(), "");

  app->begin([](bool active, float volume, const char *currentsong, int playProgressInPercent)
             {
                  if (mqtt != NULL)  {

                      if (active)
                      {
                        mqtt->publishPlaybackState(String("Playing"));
                      }
                      else
                      {
                        mqtt->publishPlaybackState(String("Stopped"));
                      }

                      mqtt->publishVolume(((int)(volume * 100)));
                      if (currentsong)
                      {
                        mqtt->publishCurrentSong(String(currentsong));
                      }

                      mqtt->publishPlayProgress(playProgressInPercent);

                      mqtt->publishBatteryVoltage(sensors->getBatteryVoltage());
                  }
                            
                  app->incrementStateVersion(); });

  leds->setBootProgress(80);

  player->begin(-1, false);

  leds->setBootProgress(90);

  // setup player, the player runs with default volume
  // TODO: Store in configuration?
  player->setVolume(0.6);

  // Start the physical button controller logic
  sensors->begin();

  // Boot complete
  leds->setBootProgress(100);

  INFO("Init finished.");
  INFO("Max  HEAP  is %d", ESP.getHeapSize());
  INFO("Free HEAP  is %d", ESP.getFreeHeap());
  INFO("Max  PSRAM is %d", ESP.getPsramSize());
  INFO("Free PSRAM is %d", ESP.getFreePsram());

  leds->setState(PLAYER_STATUS);

  // xTaskCreate(wifiscannertask, "WiFi scanner", 2048, NULL, 10, NULL);
}

void wifiConnected()
{
  IPAddress ip = WiFi.localIP();

  INFO("Connected to WiFi network. Local IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  if (settings->isMQTTEnabled())
  {
    mqtt->begin(settings->getMQTTServer(), settings->getMQTTPort(), settings->getMQTTUsername(), settings->getMQTTPassword());
  }

  // Start webserver, as we now have a WiFi stack...
  frontend->begin();

  app->announceMDNS();
  app->announceSSDP();

  if (settings->isVoiceAssistantEnabled())
  {
    INFO("Starting voice assistant integration");
    assistant->begin(settings->getVoiceAssistantServer(), settings->getVoiceAssistantPort(), settings->getVoiceAssistantAccessToken(), app->computeUUID(), [](HAState state)
                     {
                        if (state == AUTHENTICATED || state == FINISHED) {
                            INFO("Got state change from VoiceAssistant, starting a new pipeline");
                            assistant->reset();
                            assistant->startPipeline(true);
                        } }, [](String urlToPlay)
                     { 
                          INFO("Playing feedback url %s", urlToPlay.c_str()); 
                          player->playURL(urlToPlay, true); });
  }

  leds->setState(PLAYER_STATUS);

  INFO("Init done");
  INFO("Max  HEAP  is %d", ESP.getHeapSize());
  INFO("Free HEAP  is %d", ESP.getFreeHeap());
  INFO("Max  PSRAM is %d", ESP.getPsramSize());
  INFO("Free PSRAM is %d", ESP.getFreePsram());
}

void loop()
{
  // Bluetooth Sink Pairing in Progress?
  if (bluetoothsink != NULL)
  {
    int pincode = bluetoothsink->pinCode();
    if (pincode != 0 && !app->isBluetoothSpeakerConnected())
    {
      INFO("Got PIN code %d for confirmation", pincode);
      if (sensors->isPreviousPressed())
      {
        bluetoothsink->confirmPinCode();
      } 
      else
      {
        leds->setState(BTCONNECTING);
      }
    }
  }

  // dnsServer.processNextRequest();
  if (settings->isWiFiEnabled() && wifiClient != NULL)
  {
    if (WiFi.isConnected() && !app->isWifiConnected())
    {
      settings->writeToConfig();

      wifiConnected();

      app->setWifiConnected();
    }

    long now = millis();

    if (!WiFi.isConnected())
    {
      if (now - startupTime > 30000)
      {
        INFO("WiFi is not connected, so reinit the connection");
        // More than 30 seconds no WiFi connect, we reset the stored bssid
        settings->resetStoredBSSIDAndReconfigureWiFi();

        // Start timeout again
        startupTime = now;
      }
    }
    else
    {
      mqtt->loop();

      frontend->loop();
    }
  }

  leds->loop(settings->isWiFiEnabled(), app->isWifiConnected(), app->isActive(), (int)(app->getVolume() * 100), app->playProgressInPercent());

  // The hole thing here is that the audiolib and the audioplayer are not
  // thread safe !!! So we perform everything related to audio processing
  // sequentially here

  // Record a time slice
  if (settings->isVoiceAssistantEnabled() && assistant != NULL)
  {
    assistant->processAudioData();
  }

  // The main app loop
  app->loop();

  // Check for button input
  sensors->loop();

  // Check if there is a command in the command queue
  CommandData command;
  // This call is non-blocking, last parameter is xTicksToWait = 0
  // TODO: Put this into the main app loop
  int ret = xQueueReceive(commandsHandle, &command, 0);
  if (ret == pdPASS)
  {
    // We got comething from the queue
    if (command.version == COMMAND_VERSION)
    {
      if (command.command == COMMAND_PLAY_DIRECTORY)
      {
        String path(String((char *)&command.path[0]));
        // In BT Mode we always play with 100% volume, as the volume is controlled by the headphones
        int volume = app->isBluetoothSpeakerConnected() ? 100 : (int)command.volume;
        INFO("Playing %s from index %d with volume %d", path.c_str(), (int)command.index, volume);

        app->setVolume(volume / 100.0);

        app->play(path, command.index);
      }
      else
      {
        WARN("Unknown command : %d", command.command);
      }
    }
    else
    {
      WARN("Unknown version : %d", command.version);
    }
  }

  esp_task_wdt_reset();
}