#include <WiFi.h>
#include <Wire.h>

#include <esp_task_wdt.h>

#include <AudioTools.h>
#include <AudioLibs/AudioBoardStream.h>
#include <AudioCodecs/CodecMP3Helix.h>
#include <vector>
#include <mutex>
#include <ArduinoJson.h>
// #include <DNSServer.h>

#include "settings.h"
#include "tagscanner.h"
#include "app.h"
#include "frontend.h"
#include "mediaplayer.h"
#include "mediaplayersource.h"
#include "commands.h"
#include "button.h"
#include "logging.h"
#include "voiceassistant.h"
#include "pins.h"

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

String tagtopic;
String tagscannertopic;
String currentsongtopic;
String playbackstatetopic;
String volumestatetopic;

Settings settings(&SD_MMC, CONFIGURATION_FILE);

TagScanner *tagscanner = new TagScanner(&Wire1, PN532_IRQ, PN532_RST);
VoiceAssistant *assistant = new VoiceAssistant(&kit);
App *app = new App(tagscanner, &source, &player, &settings);
Frontend *frontend = new Frontend(&SD_MMC, app, HTTP_SERVER_PORT, MP3_FILE, &settings);

std::vector<CommandData> commands;
std::mutex commandsmutex;

Button play(GPIO_STARTSTOP, 300, [](ButtonAction action)
            {
  if (action == PRESSED)
  {
    app->toggleActiveState();
  } });

Button prev(GPIO_PREVIOUS, 300, [](ButtonAction action)
            {
  if (action == RELEASED) 
  {
    app->previous();
  }
  if (action == PRESSED_FOR_LONG_TIME)
  {
      float volume = app->getVolume();
      if (volume >= 0.02) 
      {
        INFO("Decrementing volume");
        app->setVolume(volume - 0.02);
      } else{
        INFO("Minimum volume reached");
      }
  } });

Button next(GPIO_NEXT, 300, [](ButtonAction action)
            {
  if (action == RELEASED)
  {
    app->next();
  }
  if (action == PRESSED_FOR_LONG_TIME)
  {
      float volume = app->getVolume();
      if (volume <= 0.98) 
      {
        INFO("Incrementing volume");
        app->setVolume(volume + 0.02);
      } else {
        INFO("Maximum volume reached");
      }
  } });

void buttonchecktask(void *arguments)
{
  INFO("Button check task started");
  while (true)
  {
    esp_task_wdt_reset();

    play.loop();
    prev.loop();
    next.loop();

    delay(15);
  }
}

void voiceassistanttask(void *arguments)
{
  INFO("Voice assist task started");

  bool wificonnected = false;

  while (true)
  {
    if (app->isWifiConnected())
    {
      if (!wificonnected)
      {
        wificonnected = true;
        assistant->begin(settings.getVoiceAssistantServer(), settings.getVoiceAssistantPort(), settings.getVoiceAssistantAccessToken(), [](HAState state)
                         {
                            if (state == AUTHENTICATED || state == FINISHED) {
                                INFO("Got state change from VoiceAssistant, starting a new pipeline");
                                assistant->reset();
                                assistant->startPipeline(true);
                            } });
      }
      assistant->loop();
      vTaskDelay(1);      
    }
    else {
      delay(200);
    }
  }
}

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
}

void callbackPrintMetaData(MetaDataType type, const char *str, int len)
{
  INFO_VAR("Detected Metadata %s : %s", toStr(type), str);
}

void setup()
{
  Serial.begin(115200);

  INFO("setup started!");

  esp_task_wdt_init(30, true); // 30 Sekunden Timeout
  esp_task_wdt_add(NULL);

  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  app->setDeviceType("ESP32 Musikbox");
  app->setName(DEVICENAME);
  app->setManufacturer("Mirko Sertic");
  app->setVersion("v1.0");
  app->setServerPort(HTTP_SERVER_PORT);

  // setup output
  auto cfg = kit.defaultConfig(RXTX_MODE);
  // sd_active is setting up SPI with the right SD pins by calling
  // SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  cfg.sd_active = false;              // We are running in SD 1bit mode, so no init needs to be done here!
  cfg.input_device = ADC_INPUT_LINE2; // input from microphone

  kit.begin(cfg);

  // AT THIS POINT THE SD CARD IS PROPERLY CONFIGURED
  source.begin();

  if (!settings.readFromConfig())
  {
    // No configuration available.
    settings.initializeWifiFromSettings();
  }
  else
  {
    settings.initializeWifiFromSettings();
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(app->computeTechnicalName().c_str());
  WiFi.setAutoReconnect(true);
  // WiFi.softAP(app->computeTechnicalName().c_str());

  // dnsServer.start(53, "*", apIP);

  app->setMQTTBrokerHost(settings.getMQTTServer());
  app->setMQTTBrokerUsername(settings.getMQTTUsername());
  app->setMQTTBrokerPassword(settings.getMQTTPassword());
  app->setMQTTBrokerPort(settings.getMQTTPort());

  // Now we initialize the frontend with its webserver and routing
  frontend->initialize();
  frontend->begin();

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

  Wire1.begin(WIRE_SDA, WIRE_SCL, 100000); // Scan ok

  INFO("NFC reader init");
  tagscanner->begin([](bool authenticated, bool knownTag, uint8_t *uid, String uidStr, uint8_t uidlength, String tagName, TagData tagdata)
                    {
    if (authenticated) 
    {
      // A tag was detected
      app->MQTT_publish(tagscannertopic, tagName);

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

        std::lock_guard<std::mutex> lck(commandsmutex);
        commands.push_back(command);

        JsonDocument result;
        result["UID"] = uidStr;

        String target;
        serializeJson(result, target);

        app->MQTT_publish(tagtopic, target);
      }
    }
    else {
      // Authentication error
      app->MQTT_publish(tagscannertopic, "Authentication error");

      app->noTagPresent();
    } }, []()
                    {
    // No tag currently detected
    app->MQTT_publish(tagscannertopic, "");

    app->noTagPresent(); });

  INFO("NFC reader init finished");

  source.setChangeIndexCallback([](Stream *next)
                                { 
                                  if (next != nullptr)
                                  {
                                    const char *songinfo = source.toStr();
                                    if (songinfo)
                                    {
                                      app->MQTT_publish(currentsongtopic, String(songinfo));
                                    }

                                    app->incrementStateVersion(); 
                                  } });

  player.setChangCallback([]()
                          {
                            if (player.isActive())
                            {
                              app->MQTT_publish(playbackstatetopic, String("Playing"));
                            }
                            else
                            {
                              app->MQTT_publish(playbackstatetopic, String("Stopped"));
                            }

                            app->MQTT_publish(volumestatetopic, String((int)(player.volume() * 100)));

                            const char *songinfo = source.toStr();
                            if (songinfo)
                            {
                              app->MQTT_publish(currentsongtopic, String(songinfo));
                            }
                            
                            app->incrementStateVersion(); });

  // player.begin();
  player.setActive(false);

  // setup player
  player.setVolume(0.5);

  // Init file browser
  strcpy(app->getCurrentPath(), "");

  INFO("Init finish");

  xTaskCreate(buttonchecktask, "Button checker", 2048, NULL, 10, NULL);
  xTaskCreate(wifiscannertask, "WiFi scanner", 2048, NULL, 10, NULL);
  if (settings.isVoiceAssistantEnabled())
  {
    //xTaskCreate(voiceassistanttask, "Voice assistant", 2048, NULL, 5, NULL);
  }
}

void wifiConnected()
{
  IPAddress ip = WiFi.localIP();

  INFO_VAR("Connected to WiFi network. Local IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  if (settings.isMQTTEnabled())
  {
    app->MQTT_init();

    playbackstatetopic = app->MQTT_announce_sensor("playbackstate", "Playback", "mdi:information-outline");

    volumestatetopic = app->MQTT_announce_number("volume", "Volume", "mdi:volume-source", "slider", 0, 100, [](String newvalue)
                                                 {
      float volume = newvalue.toFloat();
      app->setVolume(volume / 100.0); });

    app->MQTT_announce_button("startstop", "Start/Stop", "mdi:restart", []()
                              { app->toggleActiveState(); });

    app->MQTT_announce_button("next", "Next Title", "mdi:skip-next", []()
                              { app->next(); });

    app->MQTT_announce_button("previous", "Previous Title", "mdi:skip-previous", []()
                              { app->previous(); });

    tagtopic = app->MQTT_announce_tagscanner("tags");

    tagscannertopic = app->MQTT_announce_sensor("tagscanner", "Detected Card", "mdi:tag-outline");

    currentsongtopic = app->MQTT_announce_sensor("currentsongstate", "Current Song", "mdi:information-outline");
  }

  app->announceMDNS();
  app->announceSSDP();

  INFO("Init done");
}

void loop()
{
  // dnsServer.processNextRequest();

  if (settings.isWiFiEnabled() && WiFi.status() == WL_CONNECTED && !app->isWifiConnected())
  {
    wifiConnected();
    app->setWifiConnected();

    settings.writeToConfig();
  }

  app->loop();

  std::lock_guard<std::mutex> lck(commandsmutex);
  if (commands.size() > 0)
  {
    CommandData command = commands[0];
    commands.clear();

    if (command.version == COMMAND_VERSION)
    {
      if (command.command == COMMAND_PLAY_DIRECTORY)
      {
        String path(String((char *)&command.path[0]));
        INFO_VAR("Playing %s from index %d with volume %d", path.c_str(), (int)command.index, (int)command.volume);

        player.setVolume(command.volume / 100.0);
        player.setActive(false);

        strcpy(app->getCurrentPath(), path.c_str());

        source.setPath(app->getCurrentPath());
        player.begin(command.index, true);
        player.setActive(true);
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