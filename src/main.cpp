#include <WiFi.h>
#include <Wire.h>

#include <esp_task_wdt.h>

#include <AudioTools.h>
#include <AudioLibs/AudioBoardStream.h>
#include <AudioCodecs/CodecMP3Helix.h>
#include <ArduinoJson.h>
#include <Driver.h>
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

WiFiClient wifiClient;

Settings settings(&SD_MMC, CONFIGURATION_FILE);

TagScanner *tagscanner = new TagScanner(&Wire1, PN532_IRQ, PN532_RST);
App *app = new App(wifiClient, tagscanner, &source, &player, &settings);
Frontend *frontend = new Frontend(&SD_MMC, app, HTTP_SERVER_PORT, MP3_FILE, &settings);

VoiceAssistant *assistant = new VoiceAssistant(&kit);

QueueHandle_t commandsHandle;

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
        kit.setVolume(volume - 0.02);        
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
        kit.setVolume(volume + 0.02);
      } else {
        INFO("Maximum volume reached");
      }
  } });

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

  // setup output
  AudioInfo info(44100, 2, 16);
  auto cfg = kit.defaultConfig(RXTX_MODE);
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
  kit.board().getDriver()->setInputVolume(100);
  kit.setVolume(0.3);

  AudioInfo kitinfo = kit.audioInfo();
  AudioInfo kitoutinfo = kit.audioInfoOut();
  INFO_VAR("Kit Input is running with Samplerate=%d, Channels=%d and Bits per sample=%d", kitinfo.sample_rate, kitinfo.channels, kitinfo.bits_per_sample);
  INFO_VAR("Kit Output is running with Samplerate=%d, Channels=%d and Bits per sample=%d", kitoutinfo.sample_rate, kitoutinfo.channels, kitoutinfo.bits_per_sample);

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
  decoder.driver()->setInfoCallback([](MP3FrameInfo &info, void *ref)
                                    { INFO_VAR("Got libHelix Info %d bitrate %d bits per sample %d channels", info.bitrate, info.bitsPerSample, info.nChans); }, nullptr);

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
  player.setVolume(kit.getVolume());

  // Init file browser
  strcpy(app->getCurrentPath(), "");

  INFO("Init finish");

  // xTaskCreate(wifiscannertask, "WiFi scanner", 2048, NULL, 10, NULL);
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

  if (settings.isVoiceAssistantEnabled())
  {
    INFO("Starting voice assistant integration");
    assistant->begin(settings.getVoiceAssistantServer(), settings.getVoiceAssistantPort(), settings.getVoiceAssistantAccessToken(), app->computeUUID(), [](HAState state)
                     {
                        if (state == AUTHENTICATED || state == FINISHED) {
                            INFO("Got state change from VoiceAssistant, starting a new pipeline");
                            assistant->reset();
                            assistant->startPipeline(true);
                        } });
  }
  INFO("Init done");
}

long lastbuttonchecktime = millis();

void loop()
{
  // dnsServer.processNextRequest();

  if (settings.isWiFiEnabled() && WiFi.status() == WL_CONNECTED && !app->isWifiConnected())
  {
    settings.writeToConfig();

    wifiConnected();

    app->setWifiConnected();
  }

  // The hole thing here is that the audiolib and the audioplayer are not
  // thread safe !!! So we perform everything related to audio processing
  // sequentially here

  long currenttime = millis();
  if (currenttime - lastbuttonchecktime > 15)
  {
    // Perform a button check every 15ms
    play.loop();
    prev.loop();
    next.loop();
    lastbuttonchecktime = currenttime;
  }

  // Record a time slice
  if (settings.isVoiceAssistantEnabled())
  {
    int available = kit.available();
    const int maxsize = assistant->getRecordingBlockSize();
    if (available >= maxsize)
    {
      AudioBuffer buffer;
      size_t read = kit.readBytes(&(buffer.data[0]), maxsize);
      if (read > 0)
      {
        // Do something with the audio data, e.g. send it to voice assistant
        buffer.size = read;
        assistant->processAudioData(&buffer);
      }
    }
  }

  // The main app loop
  app->loop();

  AudioInfo from = player.audioInfo();
  DEBUG_VAR("Player is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);

  // Check if there is a command in the command queue
  CommandData command;
  // This call is non-blocking, last parameter is xTicksToWait = 0
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