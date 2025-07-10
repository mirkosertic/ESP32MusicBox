#include "rfidplayermode.h"

#include "globals.h"
#include "logging.h"
#include "pins.h"

RfidPlayerMode *globalRfidPlayerMode = NULL;

RfidPlayerMode::RfidPlayerMode(Leds *leds, Sensors *sensors)
	: Mode(leds, sensors) {
	globalRfidPlayerMode = this;
	this->bluetoothSpeakerConnected = false;
	this->wifiinitialized = false;
}

void RfidPlayerMode::setup() {
	Mode::setup();

	this->wifiClient = new WiFiClient();

	INFO("Initializing core components")
	this->sourceSD = new SDMediaPlayerSource(STARTFILEPATH, MP3_FILE, true);
	URLStream stream(*this->wifiClient);
	this->sourceURL = new URLMediaPlayerSource(stream, "", 0);
	this->decoder = new MP3DecoderHelix();
	this->player = new MediaPlayer(*this->sourceSD, *this->sourceURL, *this->i2sstream, *this->decoder);

	// Inform the player what to output
	this->player->setAudioInfo(defaultAudioInfo);

	this->tagscanner = new TagScanner(&Wire1, GPIO_PN532_IRQ, GPIO_PN532_RST);
	this->app = new App(this->leds, this->tagscanner, this->player, this->settings);
	INFO("Core components created. Free HEAP is %d", ESP.getFreeHeap());

	INFO("Free HEAP is %d", ESP.getFreeHeap());

	this->commandsHandle = xQueueCreate(10, sizeof(CommandData));
	if (this->commandsHandle == NULL) {
		WARN("Command queue could not be created. Halt.");
		while (true)
			;
	}

	// esp_task_wdt_init(30, true); // 30 Sekunden Timeout
	// esp_task_wdt_add(NULL);
	this->app->setDeviceType("ESP32 Musikbox");
	this->app->setName(this->settings->getDeviceName());
	this->app->setManufacturer("Mirko Sertic");
	this->app->setVersion("v1.0");
	this->app->setServerPort(HTTP_SERVER_PORT);

	leds->setBootProgress(10);

	// AT THIS POINT THE SD CARD IS PROPERLY CONFIGURED
	this->sourceSD->begin();

	leds->setBootProgress(30);

	leds->setBootProgress(40);

	// Button-Feedback goes to the app
	this->sensors->begin(this->app);

	INFO("WiFi configuration and creating networking components. Free HEAP is %d", ESP.getFreeHeap());
	this->webserver = new Webserver(&SD, this->app, HTTP_SERVER_PORT, MP3_FILE, this->settings);
	if (this->settings->isVoiceAssistantEnabled()) {
		INFO("Initializing voice assistant client. Free HEAP is %d", ESP.getFreeHeap());
		this->assistant = new VoiceAssistant(this->i2sstream, this->settings);
	}
	this->mqtt = new MQTT(*wifiClient, this->app);

	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.setHostname(app->computeTechnicalName().c_str());
	WiFi.setAutoReconnect(true);

	this->settings->initializeWifiFromSettings();
	this->wifienabled = this->settings->isWiFiEnabled();

	INFO("Bluetooth initializing buffers. Free HEAP is %d", ESP.getFreeHeap());
	this->buffer = new BufferRTOS<uint8_t>(0);
	this->buffer->resize(5 * 1024);
	this->bluetoothout = new QueueStream<uint8_t>(*this->buffer);

	INFO("Bluetooth source configuration. Free HEAP is %d", ESP.getFreeHeap());
	this->bluetoothsource = new BluetoothSource( // Connection state callback
		[this](BluetoothSource *source, esp_a2d_connection_state_t state) {
        switch (state)
        {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            INFO("bluetooth() - DISCONNECTED");
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            INFO("bluetooth() - CONNECTING");
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            INFO("bluetooth() - CONNECTED. Sending player output to Bluetooth device");
            this->leds->setState(BTCONNECTED);
            this->player->setOutput(*this->bluetoothout);
            // Bluetooth Playback is always 70%
            this->player->setVolume(0.7);
			this->bluetoothSpeakerConnected = true;
            this->leds->setBluetoothSpeakerConnected();
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            INFO("bluetooth() - DI‚SCONNECTING. Sending player output to I2S");
            this->player->setOutput(*this->i2sstream);
            break;
        } },
		// SSID pairing callback
		[](const char *ssid, esp_bd_addr_t address, int rrsi) {
            INFO("bluetooth() - Found SSID %s with RRSI %d", ssid, rrsi);
            if (globalRfidPlayerMode->settings->isValidDeviceToPairForBluetooth(String(ssid))) 
            {
                INFO("bluetooth() - Candidate for pairing found!");
                return true;
            }
            return false; },
		// AVRC callback
		[](uint8_t key, bool isReleased) {
            if (isReleased)
            {
                INFO("bluetooth() - AVRC button %d released!", key);
            }
            else
            {
                INFO("bluetooth() - AVRC button %d pressed!", key);
                if (key == 0x44) // PLAY
                {
                    INFO("bluetooth() - AVRC PLAY pressed");
                    globalRfidPlayerMode->app->toggleActiveState();
                }
                if (key == 0x45) // STOP
                {
                    INFO("bluetooth() - AVRC STOP pressed");
                    globalRfidPlayerMode->app->toggleActiveState();
                }
                if (key == 0x46) // PAUSE
                {
                    INFO("bluetooth() - AVRC PAUSE pressed");
                    globalRfidPlayerMode->app->toggleActiveState();
                }
                if (key == 0x4b) // NEXT
                {
                    INFO("bluetooth() - AVRC NEXT pressed");
                    globalRfidPlayerMode->app->next();
                }
                if (key == 0x4c) // PREVIOUS
                {
                    INFO("bluetooth() - AVRC PREVIOUS pressed");
                    globalRfidPlayerMode->app->previous();
                }
                } },
		// Read data callback
		[](uint8_t *data, int32_t len) {
			if (globalRfidPlayerMode->buffer->available() < len) {
				// Need more data
				return 0;
			}
			DEBUG("Requesting %d bytes for Bluetooth", len);
			int32_t result_bytes = globalRfidPlayerMode->buffer->readArray(data, len);
			DEBUG("Transfering %d bytes over Bluetooth, requested were %d", result_bytes, len);
			return result_bytes;
		});

	// Start output when buffer is 95% full
	this->bluetoothout->begin(95);

	this->bluetoothsource->start(this->app->computeTechnicalName());

	INFO("Bluetooth source initialized. Free HEAP is %d", ESP.getFreeHeap());
	// WiFi.softAP(app->computeTechnicalName().c_str());

	// dnsServer.start(53, "*", apIP);

	leds->setBootProgress(50);

	this->player->setMetadataCallback([](MetaDataType type, const char *str, int len) {
		INFO("Detected Metadata %s : %s", toStr(type), str);
	});

	this->decoder->driver()->setInfoCallback([](MP3FrameInfo &info, void *ref) { INFO("Got libHelix Info %d bitrate %d bits per sample %d channels", info.bitrate, info.bitsPerSample, info.nChans); }, nullptr);

	this->leds->setBootProgress(60);

	// We are running in RFID box mode
	INFO("NFC reader init");
	this->tagscanner->begin([this](bool authenticated, bool knownTag, uint8_t *uid, String uidStr, uint8_t uidlength, String tagName, TagData tagdata) {
        if (authenticated) 
        {
            // A tag was detected
            if (this->mqtt != NULL) {
                this->mqtt->publishTagScannerInfo(tagName);
            }

            this->app->setTagData(knownTag, tagName, uid, uidlength, tagdata);

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
            int ret = xQueueSend(this->commandsHandle, (void *)&command, 0);
            if (ret == pdTRUE) {
                // No problem here  
            } else if (ret == errQUEUE_FULL) {
                WARN("Unable to send data into the command queue");
            } 

            JsonDocument result;
            result["UID"] = uidStr;

            String target;
            serializeJson(result, target);

            if (this->mqtt != NULL) {
                this->mqtt->publishScannedTag(target);
            }

            this->leds->setState(CARD_DETECTED);
        }
        else
        {
            this->leds->setState(CARD_ERROR);
        }
    }
    else {
        // Authentication error
        if (this->mqtt != NULL) {
            this->mqtt->publishTagScannerInfo("Authentication error");
        }

        this->app->noTagPresent();

        this->leds->setState(CARD_ERROR);
    } }, [this]() {
        // No tag currently detected
        if (this->mqtt != NULL)  {
            this->mqtt->publishTagScannerInfo("");
        }

        this->app->noTagPresent(); });

	this->leds->setBootProgress(70);
	INFO("NFC reader init finished");

	this->sourceSD->setChangeIndexCallback([this](Stream *next) {
        INFO("In ChangeIndex callback");
        if (next != nullptr)
        {
            const char *songinfo = this->sourceSD->toStr();
            if (songinfo && this->mqtt != NULL)
            {
	            this->mqtt->publishCurrentSong(String(songinfo));
            }

            this->app->incrementStateVersion();
        }
        INFO("Done"); });

	// Init file browser
	strcpy(this->player->getCurrentPath(), "");

	this->app->begin([this](bool active, float volume, const char *currentsong, int playProgressInPercent) {
        if (this->mqtt != NULL)
        {

            if (active)
            {
                this->mqtt->publishPlaybackState(String("Playing"));
            }
            else
            {
                this->mqtt->publishPlaybackState(String("Stopped"));
            }

            this->mqtt->publishVolume(((int)(volume * 100)));
            if (currentsong)
            {
                this->mqtt->publishCurrentSong(String(currentsong));
            }

            this->mqtt->publishPlayProgress(playProgressInPercent);

            this->mqtt->publishBatteryVoltage(this->sensors->getBatteryVoltage());
        }

        this->app->incrementStateVersion(); });

	this->leds->setBootProgress(80);

	this->player->begin(-1, false);

	this->leds->setBootProgress(90);

	// setup player, the player runs with default volume
	// TODO: Store in configuration?
	this->player->setVolume(0.6);

	// Start the physical button controller logic
	this->sensors->begin(this->app);

	// Boot complete
	this->leds->setBootProgress(100);

	this->leds->setState(PLAYER_STATUS);
}

void RfidPlayerMode::wifiConnected() {

	INFO("Connected to WiFi network.");
	INFO(" + Local IP : %s ", WiFi.localIP().toString().c_str());
	INFO(" + Gateway  : %s", WiFi.gatewayIP().toString().c_str());
	INFO(" + Subnet   : %s", WiFi.subnetMask().toString().c_str());
	INFO(" + BSSID    : %s", WiFi.BSSIDstr().c_str());
	INFO(" + Channel  : %d", WiFi.channel());
	INFO(" + RSSI     : %d", WiFi.RSSI());

	// Start webserver, as we now have a WiFi stack...
	this->webserver->begin();

	if (this->settings->isMQTTEnabled()) {
		// Bootstrap MQTT logic...
		this->mqtt->begin(this->settings->getMQTTServer(), this->settings->getMQTTPort(), this->settings->getMQTTUsername(), this->settings->getMQTTPassword(), this->webserver->getConfigurationURL());
	}

	if (this->settings->isVoiceAssistantEnabled() && this->assistant != NULL) {
		INFO("Starting voice assistant integration");
		this->assistant->begin(this->settings->getVoiceAssistantServer(), this->settings->getVoiceAssistantPort(), this->settings->getVoiceAssistantAccessToken(), this->app->computeUUID(), [this](HAState state) {
                            if (state == AUTHENTICATED || state == FINISHED) {
                                INFO("Got state change from VoiceAssistant, starting a new pipeline");
                                this->assistant->reset();
                                this->assistant->startPipeline(true);
                            } }, [this](String urlToPlay) { 
                            INFO("Playing feedback url %s", urlToPlay.c_str()); 
                            this->player->playURL(urlToPlay, true); });
	}

	this->leds->setState(PLAYER_STATUS);

	this->wifiinitialized = true;

	INFO("WiFi connected");
	INFO("Max  HEAP  is %d", ESP.getHeapSize());
	INFO("Free HEAP  is %d", ESP.getFreeHeap());
	INFO("Max  PSRAM is %d", ESP.getPsramSize());
	INFO("Free PSRAM is %d", ESP.getFreePsram());
}

ModeStatus RfidPlayerMode::loop() {
	Mode::loop();

	// dnsServer.processNextRequest();
	if (this->wifienabled && this->wifiClient != NULL) {
		if (WiFi.isConnected() && !this->wifiinitialized) {
			INFO("WiFi connection established");

			this->settings->writeToConfig();

			this->wifiConnected();
		}

		long now = millis();
		static long lastchecktime = millis();

		if (!WiFi.isConnected()) {
			if (now - lastchecktime > 30000) {
				INFO("WiFi connection timeout. Disabling WiFi for now.");
				// More than 30 seconds no WiFi connect, we reset the stored bssid
				WiFi.disconnect();
				WiFi.setSleep(true);
				this->wifienabled = false;

				// Start timeout again
				lastchecktime = now;
			}
		} else {
			this->mqtt->loop();

			this->webserver->loop();

			// The hole thing here is that the audiolib and the audioplayer are not
			// thread safe !!! So we perform everything related to audio processing
			// sequentially here

			// Record a time slice
			if (this->settings->isVoiceAssistantEnabled() && this->assistant != NULL) {
				this->assistant->processAudioData();
			}
		}
	}

	this->leds->loop(this->wifienabled, WiFi.isConnected(), app->isActive(), (int) (app->getVolume() * 100), app->playProgressInPercent());

	// The main app loop
	this->app->loop();

	// Check if there is a command in the command queue
	CommandData command;
	// This call is non-blocking, last parameter is xTicksToWait = 0
	// TODO: Put this into the main app loop
	int ret = xQueueReceive(this->commandsHandle, &command, 0);
	if (ret == pdPASS) {
		// We got comething from the queue
		if (command.version == COMMAND_VERSION) {
			if (command.command == COMMAND_PLAY_DIRECTORY) {
				String path(String((char *) &command.path[0]));
				// In BT Mode we always play with 100% volume, as the volume is controlled by the headphones
				int volume = this->bluetoothSpeakerConnected ? 100 : (int) command.volume;
				INFO("Playing %s from index %d with volume %d", path.c_str(), (int) command.index, volume);

				this->app->setVolume(volume / 100.0);

				this->app->play(path, command.index);
			} else {
				WARN("Unknown command : %d", command.command);
			}
		} else {
			WARN("Unknown version : %d", command.version);
		}
	}

	esp_task_wdt_reset();

	if (this->app->isActive()) {
		return MODE_NOT_IDLE;
	}

	return MODE_IDLE;
}

void RfidPlayerMode::prepareDeepSleep() {
	this->tagscanner->prepareDeepSleep();
	if (WiFi.isConnected()) {
		WiFi.disconnect();
	}
}
