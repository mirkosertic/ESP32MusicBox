#include <WiFi.h>
#include <Wire.h>
#include <SD.h>
#include <SPIFFS.h>

#include <esp_task_wdt.h>

#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <BluetoothA2DPSource.h>
#include <ArduinoJson.h>
#include <Driver.h>
#include <esp_idf_version.h>
#include <esp_arduino_version.h>

// #include <DNSServer.h>

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

// BluetoothA2DPSource *a2dpsource;
bool btspeakermode = false;

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

int32_t callbackGetSoundData(uint8_t *data, int32_t len)
{
  // generate your sound data
  // return the effective length in bytes
  return 0;
}

void setup()
{
  Serial.begin(115200);

  INFO("Setup started");
  INFO("Running on Arduino : %d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  INFO("Running on ESP IDF : %s", esp_get_idf_version());
  INFO("Free HEAP is %d", ESP.getFreeHeap());

  btspeakermode = false;

  INFO("Creating core components")
  source = new MediaPlayerSource(STARTFILEPATH, MP3_FILE, true);
  i2s = new I2SStream();
  decoder = new MP3DecoderHelix();
  player = new MediaPlayer(*source, *i2s, *decoder);

  settings = new Settings(&SD, CONFIGURATION_FILE, btspeakermode);

  tagscanner = new TagScanner(&Wire1, GPIO_PN532_IRQ, GPIO_PN532_RST);
  app = new App(tagscanner, source, player, settings, player);
  leds = new Leds(app);
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
    {
      delay(1000);
    }
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
  AudioInfo info(44100, 2, 16);
  auto cfg = i2s->defaultConfig(TX_MODE);
  cfg.pin_bck = GPIO_I2S_BCK;
  cfg.pin_ws = GPIO_I2S_WS;
  cfg.pin_data = GPIO_I2S_DATA;
  cfg.i2s_format = I2S_STD_FORMAT; // default format
  cfg.copyFrom(info);

  INFO("i2s sound system init");
  if (!i2s->begin(cfg))
  {
    WARN("Could not start i2s sound system!");
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
  if (!SD.begin(GPIO_SPI_SS, SPI))
  {
    WARN("Could not enable SD-Card over SPI!");
    while (true)
      ;
  }
  leds->setBootProgress(20);

  // AT THIS POINT THE SD CARD IS PROPERLY CONFIGURED
  source->begin();

  leds->setBootProgress(30);

  INFO("Retrieving system configuration");
  if (!settings->readFromConfig())
  {
    // No configuration available.
    settings->initializeWifiFromSettings();
  }
  else
  {
    if (sensors->isStartStopPressed())
    {
      settings->resetStoredBSSID();
    }
    settings->initializeWifiFromSettings();
  }

  leds->setBootProgress(40);

  if (!btspeakermode)
  {
    INFO("WiFi configuration and creating networking components. Free HEAP is %d", ESP.getFreeHeap());
    wifiClient = new WiFiClient();
    frontend = new Frontend(&SD, app, HTTP_SERVER_PORT, MP3_FILE, settings);
    assistant = new VoiceAssistant(i2s, settings);
    mqtt = new MQTT(*wifiClient, app);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(app->computeTechnicalName().c_str());
    WiFi.setAutoReconnect(true);

    INFO("Networking initialized. Free HEAP is %d", ESP.getFreeHeap());
  }
  else
  {
    /*  INFO("Bluetooth configuration. Free HEAP is %d", ESP.getFreeHeap());
      a2dpsource = new BluetoothA2DPSource();
      a2dpsource->set_local_name(app->computeTechnicalName().c_str());
      a2dpsource->clean_last_connection();
      a2dpsource->set_reset_ble(true);
      a2dpsource->set_ssid_callback([](const char *ssid, esp_bd_addr_t address, int rrsi)
                                    {
        INFO("bluetooth() - Found SSID %s with RRSI %d", ssid, rrsi);
        return false; });
      a2dpsource->set_discovery_mode_callback([](esp_bt_gap_discovery_state_t discoveryMode)
                                              {
        switch (discoveryMode)
        {
        case ESP_BT_GAP_DISCOVERY_STARTED:
          INFO("bluetooth() - Discovery started");
          break;
        case ESP_BT_GAP_DISCOVERY_STOPPED:
          INFO("bluetooth() - Discovery stopped");
          break;
        } });
      a2dpsource->start();

      INFO("Bluetooth initialized. Free HEAP is %d", ESP.getFreeHeap());*/
  }
  // WiFi.softAP(app->computeTechnicalName().c_str());

  // dnsServer.start(53, "*", apIP);

  leds->setBootProgress(50);

  player->setMetadataCallback(callbackPrintMetaData);
  decoder->driver()->setInfoCallback([](MP3FrameInfo &info, void *ref)
                                     { INFO("Got libHelix Info %d bitrate %d bits per sample %d channels", info.bitrate, info.bitsPerSample, info.nChans); }, nullptr);

  INFO("i2c connection init");
  Wire1.begin(GPIO_WIRE_SDA, GPIO_WIRE_SCL, 100000); // Scan ok

  leds->setBootProgress(60);

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

  // Initialize Bluetooth connections
  // a2dpsink.set_auto_reconnect(false);
  // a2dp_sink.set_stream_reader(read_data_stream, false);
  // a2dpsink.start(app->computeTechnicalName().c_str());

  // Boot complete
  leds->setBootProgress(100);

  INFO("Init finished. Free HEAP is %d", ESP.getFreeHeap());

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
}

void loop()
{
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

  leds->loop();

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
        INFO("Playing %s from index %d with volume %d", path.c_str(), (int)command.index, (int)command.volume);

        app->setVolume(command.volume / 100.0);

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