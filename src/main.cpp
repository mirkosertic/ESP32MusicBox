#include <WiFi.h>
#include <Wire.h>

#include <esp_task_wdt.h>

#include <AudioTools.h>
#include <AudioTools/AudioLibs/AudioBoardStream.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
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
#include "buttons.h"
#include "logging.h"
#include "voiceassistant.h"
#include "pins.h"
#include "leds.h"

const char *CONFIGURATION_FILE = "/configuration.json";
const char *STARTFILEPATH = "/";
const char *MP3_FILE = ".mp3";
const int HTTP_SERVER_PORT = 80;

// IPAddress apIP(192, 168, 4, 1);
// DNSServer dnsServer;

MediaPlayerSource source(STARTFILEPATH, MP3_FILE, true);
AudioBoardStream kit(AudioKitEs8388V1);
MP3DecoderHelix decoder;
MediaPlayer player(source, kit, decoder);

WiFiClient wifiClient;

Settings settings(&SD_MMC, CONFIGURATION_FILE);

TagScanner *tagscanner = new TagScanner(&Wire1, PN532_IRQ, PN532_RST);
App *app = new App(wifiClient, tagscanner, &source, &player, &settings, &kit);
Frontend *frontend = new Frontend(&SD_MMC, app, HTTP_SERVER_PORT, MP3_FILE, &settings);
Leds *leds = new Leds(app);
Buttons *buttons = new Buttons(app, leds);
VoiceAssistant *assistant = new VoiceAssistant(&kit, &settings);
MQTT *mqtt = new MQTT(wifiClient, app);

QueueHandle_t commandsHandle;

long startupTime = millis();

void wifiscannertask(void *arguments)
{
  INFO("WiFi scanner task started");
  while (true)
  {
    settings.rescanForBetterNetworksAndReconfigure();
    delay(60000);
  }
}

void dummyhandler(bool, int, void *)
{
  INFO("Button pressed!");
  // player.playURL("http://192.168.0.159:8123/api/tts_proxy/a5b81f9cf47b4f7b244c110c37305dab85944c2b_de-de_9d38d7a658_tts.piper.mp3", true);
}

void callbackPrintMetaData(MetaDataType type, const char *str, int len)
{
  INFO_VAR("Detected Metadata %s : %s", toStr(type), str);
}

void setup()
{
  Serial.begin(115200);

  INFO("LED Status display init");
  leds->begin();

  INFO("setup started!");

  INFO_VAR("Running on Arduino : %d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  INFO_VAR("Running on ESP IDF : %s", esp_get_idf_version());

  commandsHandle = xQueueCreate(10, sizeof(CommandData));
  if (commandsHandle == NULL)
  {
    WARN("Command queue could not be created. Halt.");
    while (1)
    {
      delay(1000);
    }
  }

  // esp_task_wdt_init(30, true); // 30 Sekunden Timeout
  // esp_task_wdt_add(NULL);

  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  app->setDeviceType("ESP32 Musikbox");
  app->setName(DEVICENAME);
  app->setManufacturer("Mirko Sertic");
  app->setVersion("v1.0");
  app->setServerPort(HTTP_SERVER_PORT);

  leds->setState(BOOT);
  leds->setBootProgress(17);

  // setup output
  AudioInfo info(44100, 2, 16);
  auto cfg = kit.defaultConfig(RXTX_MODE);
  // auto cfg = kit.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  cfg.sd_active = false;              // We are running in SD 1bit mode, so no init needs to be done here!
  cfg.input_device = ADC_INPUT_LINE2; // input from microphone, stereo in case of AiThinker AudioKit

  if (!kit.begin(cfg))
  {
    WARN("Could not start audio kit!");
    while (true)
      ;
  }

  // Set the microphone level a little bit louder...
  // This will change the gain of the microphone
  kit.board().getDriver()->setInputVolume(90);
  kit.board().getDriver()->setVolume(90);

  // Set the overall volume of the Kit.
  kit.setVolume(0.7);

  AudioInfo kitinfo = kit.audioInfo();
  AudioInfo kitoutinfo = kit.audioInfoOut();
  INFO_VAR("Kit Input is running with Samplerate=%d, Channels=%d and Bits per sample=%d", kitinfo.sample_rate, kitinfo.channels, kitinfo.bits_per_sample);
  INFO_VAR("Kit Output is running with Samplerate=%d, Channels=%d and Bits per sample=%d", kitoutinfo.sample_rate, kitoutinfo.channels, kitoutinfo.bits_per_sample);

  // AT THIS POINT THE SD CARD IS PROPERLY CONFIGURED
  source.begin();

  leds->setBootProgress(17 * 2);

  if (!settings.readFromConfig())
  {
    // No configuration available.
    settings.initializeWifiFromSettings();
  }
  else
  {
    if (buttons->isStartStopPressed())
    {
      settings.resetStoredBSSID();
    }
    settings.initializeWifiFromSettings();
  }

  leds->setBootProgress(17 * 3);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(app->computeTechnicalName().c_str());
  WiFi.setAutoReconnect(true);
  // WiFi.softAP(app->computeTechnicalName().c_str());

  // dnsServer.start(53, "*", apIP);

  // Now we initialize the frontend with its webserver and routing
  frontend->begin();

  leds->setBootProgress(17 * 4);

  // https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit/issues/3

  // setup additional buttons
  kit.addDefaultActions();
  kit.addAction(kit.getKey(1), dummyhandler);
  kit.addAction(kit.getKey(2), dummyhandler);
  kit.addAction(kit.getKey(3), dummyhandler);
  kit.addAction(kit.getKey(4), dummyhandler);
  kit.addAction(kit.getKey(5), dummyhandler);
  kit.addAction(kit.getKey(6), dummyhandler);

  player.setMetadataCallback(callbackPrintMetaData);
  decoder.driver()->setInfoCallback([](MP3FrameInfo &info, void *ref)
                                    { INFO_VAR("Got libHelix Info %d bitrate %d bits per sample %d channels", info.bitrate, info.bitsPerSample, info.nChans); }, nullptr);

  Wire1.begin(WIRE_SDA, WIRE_SCL, 100000); // Scan ok

  INFO("NFC reader init");
  tagscanner->begin([](bool authenticated, bool knownTag, uint8_t *uid, String uidStr, uint8_t uidlength, String tagName, TagData tagdata)
                    {
    if (authenticated) 
    {
      // A tag was detected
      mqtt->publishTagScannerInfo(tagName);

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

        mqtt->publishScannedTag(target);

        leds->setState(CARD_DETECTED);
      }
      else
      {
        leds->setState(CARD_ERROR);
      }
    }
    else {
      // Authentication error
      mqtt->publishTagScannerInfo("Authentication error");

      app->noTagPresent();

      leds->setState(CARD_ERROR);
    } }, []()
                    {
    // No tag currently detected
    mqtt->publishTagScannerInfo("");

    app->noTagPresent(); });

  leds->setBootProgress(17 * 5);
  INFO("NFC reader init finished");

  source.setChangeIndexCallback([](Stream *next)
                                { 
                                  INFO("In info callback");
                                  if (next != nullptr)
                                  {
                                    const char *songinfo = source.toStr();
                                    if (songinfo)
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
                            
                            app->incrementStateVersion(); });

  player.begin(-1, false);

  // setup player, the player always runs on full volume
  // playback volume is controlled by the audiokit PA, not by the player software volume control
  player.setVolume(1.0);

  // Start the physical button controller logic
  buttons->begin();

  // Boot complete
  leds->setBootProgress(100);

  INFO("Init finish");

  leds->setState(PLAYER_STATUS);

  // xTaskCreate(wifiscannertask, "WiFi scanner", 2048, NULL, 10, NULL);
}

void wifiConnected()
{
  IPAddress ip = WiFi.localIP();

  INFO_VAR("Connected to WiFi network. Local IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  if (settings.isMQTTEnabled())
  {
    mqtt->begin(settings.getMQTTServer(), settings.getMQTTPort(), settings.getMQTTUsername(), settings.getMQTTPassword());
  }

  app->announceMDNS();
  app->announceSSDP();

  if (settings.isVoiceAssistantEnabled())
  {
    INFO("Starting voice assistant integration");
    assistant->begin(settings.getVoiceAssistantServer(), settings.getVoiceAssistantPort(), settings.getVoiceAssistantAccessToken(), app->computeUUID(), [](HAState state)
                     {
                        if (state == AUTHENTICATED || state == FINISHED) {
                            INFO("Got state change from VoiceAssistant, starting a new pipeline");
                            assistant->reset();
                            assistant->startPipeline(true);
                        } }, [](String urlToPlay)
                     { 
                          INFO_VAR("Playing feedback url %s", urlToPlay.c_str()); 
                          player.playURL(urlToPlay, true); });
  }

  leds->setState(PLAYER_STATUS);
  INFO("Init done");
}

void loop()
{
  // dnsServer.processNextRequest();
  if (settings.isWiFiEnabled())
  {
    if (WiFi.isConnected() && !app->isWifiConnected())
    {
      settings.writeToConfig();

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
        settings.resetStoredBSSIDAndReconfigureWiFi();

        // Start timeout again
        startupTime = now;
      }
    }
  }

  leds->loop();

  // The hole thing here is that the audiolib and the audioplayer are not
  // thread safe !!! So we perform everything related to audio processing
  // sequentially here

  // Record a time slice
  if (settings.isVoiceAssistantEnabled())
  {
    assistant->processAudioData();
  }

  // The main app loop
  app->loop();

  AudioInfo from = player.audioInfo();
  DEBUG_VAR("Player is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);

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
        INFO_VAR("Playing %s from index %d with volume %d", path.c_str(), (int)command.index, (int)command.volume);

        app->setVolume(command.volume / 100.0);

        app->play(path, command.index);
      }
      else
      {
        WARN_VAR("Unknown command : %d", command.command);
      }
    }
    else
    {
      WARN_VAR("Unknown version : %d", command.version);
    }
  }

  kit.processActions();

  esp_task_wdt_reset();
}