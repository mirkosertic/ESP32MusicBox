#include "voiceassistant.h"

#include <ArduinoJson.h>

#include "logging.h"

void vadispatcher(void *parameters)
{
  static long notifycounter = 0;
  VoiceAssistant *target = (VoiceAssistant *)parameters;
  INFO("Voice assistant polling task started");
  while (true)
  {
    notifycounter++;
    if (notifycounter > 1000)
    {
      INFO("I am still alive!");
      notifycounter = 0;
    }
    target->pollQueue();
    vTaskDelay(1);
  }
}

void wsdispatcher(void *parameters)
{
  VoiceAssistant *target = (VoiceAssistant *)parameters;
  INFO("WebSocket looper task started");
  long notifycounter = 0;
  while (true)
  {
    notifycounter++;
    if (notifycounter > 1000)
    {
      INFO("I am still here");
      notifycounter = 0;
    }
    target->loop();
    vTaskDelay(1);
  }
}

class VoiceAssistantStream : public AudioStream
{
private:
  VoiceAssistant *assistant;

public:
  VoiceAssistantStream(VoiceAssistant *assistant)
  {
    this->assistant = assistant;
  }

  bool begin()
  {
    return true;
  }

  void end()
  {
  }

  void setAudioInfo(AudioInfo info) override
  {
    AudioStream::setAudioInfo(info);
  }

  int availableForWrite() { return AUDIO_BUFFER_SIZE * 4; }

  /// amount of data available
  int available()
  {
    return 0;
  }

  size_t write(const uint8_t *buffer, size_t size) override
  {
    DEBUG("Writing %d bytes", size);
    this->assistant->sendAudioData(buffer, size);
    return size;
  }

  /// Read from audio data to buffer
  size_t readBytes(uint8_t *buffer, size_t size) override
  {
    return 0;
  }
};

void VoiceAssistant::webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_PING:
  {
    INFO("Ping");
    break;
  }
  case WStype_PONG:
  {
    INFO("Pong");
    break;
  }
  case WStype_DISCONNECTED:
  {
    INFO("Disconnected!");
    this->stateIs(DISCONNECTED);

    delay(100000);

    // this->connectOrReconnect();
    break;
  }
  case WStype_CONNECTED:
  {
    INFO("Connected to url: %s", payload);
    this->stateIs(CONNECTED);
    break;
  }
  case WStype_TEXT:
  {

    INFO("Event Debug: %s", payload);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      WARN("Failed to parse: %s with %s", payload, error.c_str());
    }
    else
    {
      const char *type = doc["type"];
      INFO("Got event of type: %s", type);

      if (strcmp(type, "auth_required") == 0)
      {
        INFO("Sending authentication token");
        sendAuthentication();
      }
      if (strcmp(type, "auth_ok") == 0)
      {
        const char *haversion = doc["ha_version"];
        INFO("Authenticated against Homeassistant %s", haversion);
        this->stateIs(AUTHENTICATED);
      }
      if (strcmp(type, "auth_invalid") == 0)
      {
        const char *message = doc["message"];
        WARN("Authenticated failure : %s", message);
        this->stateIs(AUTHENTICATIONERROR);
      }
      if (strcmp(type, "event") == 0)
      {
        if (strcmp(doc["event"]["type"], "run-start") == 0)
        {
          this->binaryHandler = doc["event"]["data"]["runner_data"]["stt_binary_handler_id"];
          INFO("run-start received with binary handler id %d", this->binaryHandler);
          this->stateIs(STARTED);
        }
        if (strcmp(doc["event"]["type"], "run-end") == 0)
        {
          INFO("run-end received");
          this->stateIs(FINISHED);

          if (this->urlToSpeak.length() > 0)
          {
            INFO("Playing URL %s", this->urlToSpeak.c_str());

            this->playAudioFeedbackCallback(this->urlToSpeak);
          }
        }
        if (strcmp(doc["event"]["type"], "wake_word-start") == 0)
        {
          int sampleRate = doc["event"]["data"]["metadata"]["sample_rate"];
          uint16_t channelCount = doc["event"]["data"]["metadata"]["channel"];
          uint8_t bitsPerSample = doc["event"]["data"]["metadata"]["bit_rate"];

          AudioInfo from = this->source->audioInfo();

          INFO("Source is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);
          INFO("Metadata from HA is Samplerate=%d, Channels=%d and Bits per sample=%d", sampleRate, channelCount, bitsPerSample);

          if (!this->converterStream->begin(from, this->recordingQuality))
          {
            WARN("Could not begin converter stream!");
            while (true)
              ;
          }

          AudioInfo in = this->converterStream->audioInfo();
          INFO("Converter in is Samplerate=%d, Channels=%d and Bits per sample=%d", in.sample_rate, in.channels, in.bits_per_sample);
          AudioInfo out = this->converterStream->audioInfoOut();
          INFO("Converter out is Samplerate=%d, Channels=%d and Bits per sample=%d", out.sample_rate, out.channels, out.bits_per_sample);

          INFO("wake_word-start received");
          this->stateIs(RECORDING);
        }
        if (strcmp(doc["event"]["type"], "wake_word-end") == 0)
        {
          INFO("wake_word-end received");
          this->stateIs(WAKEWORDFINISHED);
        }
        if (strcmp(doc["event"]["type"], "stt-start") == 0)
        {
          int sampleRate = doc["event"]["data"]["metadata"]["sample_rate"];
          uint16_t channelCount = doc["event"]["data"]["metadata"]["channel"];
          uint8_t bitsPerSample = doc["event"]["data"]["metadata"]["bit_rate"];

          AudioInfo from = this->source->audioInfo();

          INFO("Source is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);
          INFO("Metadata from HA is Samplerate=%d, Channels=%d and Bits per sample=%d", sampleRate, channelCount, bitsPerSample);

          if (!this->converterStream->begin(from, this->recordingQuality))
          {
            WARN("Could not begin converter stream!");
            while (true)
              ;
          }

          AudioInfo in = this->converterStream->audioInfo();
          INFO("Converter in is Samplerate=%d, Channels=%d and Bits per sample=%d", in.sample_rate, in.channels, in.bits_per_sample);
          AudioInfo out = this->converterStream->audioInfoOut();
          INFO("Converter out is Samplerate=%d, Channels=%d and Bits per sample=%d", out.sample_rate, out.channels, out.bits_per_sample);

          INFO("stt-start received");
          this->stateIs(RECORDING);
          // m_speaker->playReady();
        }
        if (strcmp(doc["event"]["type"], "stt-vad-end") == 0)
        {
          INFO("wstt-vad-end received");
          // this->stateIs(STTFINISHED);

          // finishAudioStream();
        }
        if (strcmp(doc["event"]["type"], "stt-end") == 0)
        {
          INFO("stt-end received");
          this->stateIs(STTFINISHED);

          // finishAudioStream();
        }
        if (strcmp(doc["event"]["type"], "tts-end") == 0)
        {
          this->stateIs(FINISHED);

          const char *ressource = doc["event"]["data"]["tts_output"]["url"];

          this->urlToSpeak = this->baseUrl + ressource;

          INFO("tts-end received. Playing URL %s", this->urlToSpeak.c_str());
        }
      }
    }
    break;
  }
  case WStype_BIN:
    INFO("Get binary length: %u", length);
    //			hexdump(payload, length);

    // send data to server
    // webSocket.sendBIN(payload, length);
    break;
  case WStype_ERROR:
    INFO("WStype_ERROR");
    break;
  case WStype_FRAGMENT_TEXT_START:
    INFO("WStype_FRAGMENT_TEXT_START");
    break;
  case WStype_FRAGMENT_BIN_START:
    INFO("WStype_FRAGMENT_BIN_START");
    break;
  case WStype_FRAGMENT:
    INFO("WStype_FRAGMENT");
    break;
  case WStype_FRAGMENT_FIN:
    INFO("WStype_FRAGMENT_FIN");
    break;
  }
}

void VoiceAssistant::sendAuthentication()
{
  this->stateIs(AUTHREQUESTED);
  JsonDocument auth;
  auth["type"] = "auth";
  auth["access_token"] = this->token;

  String buffer;
  size_t buffersize = serializeJson(auth, buffer);
  this->webSocket->sendTXT(buffer);
}

VoiceAssistant::VoiceAssistant(AudioStream *source, Settings *settings)
{
  this->audioBuffersHandle = xQueueCreate(4, sizeof(AudioBuffer));
  if (audioBuffersHandle == NULL)
  {
    WARN("Audio buffers queue could not be created. Halt.");
    while (1)
    {
      delay(1000);
    }
  }

  this->wsloop = NULL;

  this->state = DISCONNECTED;
  this->source = source;
  this->settings = settings;
  this->commandid = 1;
  this->webSocket = new WebSocketsClient();

  this->recordingQuality = AudioInfo(16000, 1, 16);

  this->outputdelegate = new VoiceAssistantStream(this);
  this->outputdelegate->setAudioInfo(recordingQuality);

  this->buffer = new BufferedStream(8192, *this->outputdelegate);
  this->buffer->setAudioInfo(recordingQuality);

  this->converterStream = new FormatConverterStream(*this->buffer);
  this->converterStream->setBuffered(false);

  this->started = false;

  // this->webSocket->setReconnectInterval(1000);
  // this->webSocket->enableHeartbeat(200, 200, 1);
  this->webSocket->onEvent([this](WStype_t type, uint8_t *payload, size_t length)
                           { this->webSocketEvent(type, payload, length); });
}

VoiceAssistant::~VoiceAssistant()
{
  this->webSocket->disconnect();
  delete this->webSocket;
  delete this->outputdelegate;
  delete this->converterStream;
}

void VoiceAssistant::connectOrReconnect()
{
  INFO("Connecting to WebSocket %s:%d", this->host.c_str(), this->port);
  if (this->webSocket->isConnected())
  {
    this->webSocket->disconnect();
  }

  if (this->wsloop != NULL)
  {
    INFO("Deleting WS Task");
    vTaskDelete(this->wsloop);
    delay(1000);
  }

  INFO("Begin new WS Connection");
  this->webSocket->begin(this->host, this->port, "/api/websocket");
  INFO("Starting new WS Task");
  xTaskCreate(wsdispatcher, "WebSocket", 16384, this, 10, &this->wsloop);
}

void VoiceAssistant::begin(String host, int port, String token, String deviceId, StateNotifierCallback stateNotifier, PlayAudioFeedbackCallback playFeedbackCallback)
{
  this->token = token;
  this->host = host;
  this->port = port;
  this->deviceId = deviceId;
  this->stateNotifier = stateNotifier;
  this->playAudioFeedbackCallback = playFeedbackCallback;

  this->baseUrl = String("http://") + host + ":" + port;

  this->connectOrReconnect();

  xTaskCreate(vadispatcher, "Voice Assistant", 32768, this, 10, NULL);

  this->started = true;

  INFO("Init finished");
}

void VoiceAssistant::stateIs(HAState state)
{
  this->state = state;
  this->stateNotifier(this->state);
}

void VoiceAssistant::reset()
{
  if (this->state == FINISHED)
  {
    this->state = AUTHENTICATED;
  }
}

HAState VoiceAssistant::currentState()
{
  return this->state;
}

bool VoiceAssistant::startPipeline(bool includeWakeWordDetection)
{
  this->urlToSpeak = String();
  if (this->state == AUTHENTICATED)
  {
    JsonDocument runcmd;
    runcmd["id"] = ++this->commandid;
    runcmd["type"] = "assist_pipeline/run";
    if (includeWakeWordDetection)
    {
      runcmd["start_stage"] = "wake_word";
    }
    else
    {
      runcmd["start_stage"] = "stt";
    }

    INFO("Using AudioInfo with Samplerate=%d, Channels=%d and Bits per sample=%d", this->recordingQuality.sample_rate, this->recordingQuality.channels, this->recordingQuality.bits_per_sample);

    runcmd["end_stage"] = "tts";
    runcmd["input"]["sample_rate"] = this->recordingQuality.sample_rate;
    runcmd["input"]["device_id"] = this->deviceId;

    // Details here: https://developers.home-assistant.io/docs/voice/pipelines/
    runcmd["input"]["volume_multiplier"] = this->settings->getVoiceAssistantVolumeMultiplier();
    runcmd["input"]["timeout"] = this->settings->getVoiceAssistantWakeWordTimeout();
    runcmd["input"]["auto_gain_dbfs"] = this->settings->getVoiceAssistantAutomaticGain();
    runcmd["input"]["noise_suppression_level "] = this->settings->getVoiceAssistantNoiseSuppressionLevel();

    String buffer;
    size_t buffersize = serializeJson(runcmd, buffer);

    INFO("Debug payload %s", buffer.c_str());

    this->webSocket->sendTXT(buffer);

    return true;
  }
  else
  {
    INFO("Not authenticated, cannot start pipeline");
    return false;
  }
}

void VoiceAssistant::sendAudioData(const uint8_t *data, size_t length)
{
  std::unique_lock<std::mutex> lck(this->loopmutex, std::defer_lock);
  if (this->state == RECORDING)
  {
    INFO("Sending %d bytes audio data", length);

    if (length > 0)
    {
      int transferlength = 1 + length;

      uint8_t transferbuffer[transferlength];
      transferbuffer[0] = this->binaryHandler;

      memcpy(&transferbuffer[1], data, length);

      long start = millis();
      this->webSocket->sendBIN(transferbuffer, transferlength);
      vTaskDelay(1);
      float duration = millis() - start;
      float thruputpersec = ((float)length) / duration * 1000.0;
      INFO("Thruput is %.02f bytes/second", thruputpersec);
    }
    else
    {
      WARN("Ingnoring data packet with size 0");
    }
  }
  else
  {
    INFO("Ignoring %d bytes of audio data, as we are not recording", length);
  }
}

void VoiceAssistant::finishAudioStream()
{
  std::lock_guard<std::mutex> lck(this->loopmutex);
  if (this->state == RECORDING)
  {
    INFO("Finishing stream!");
    uint8_t transferbuffer[1];
    transferbuffer[0] = this->binaryHandler;

    this->webSocket->sendBIN(&transferbuffer[0], 1);
    this->state = TTSFINISHED;
  }
  else
  {
    INFO("Nothing to do on finish!");
  }
}

void VoiceAssistant::pollQueue()
{
  AudioBuffer buffer;
  int ret = xQueueReceive(this->audioBuffersHandle, &buffer, 0);
  while (ret == pdPASS)
  {
    //  Write data to converter stream
    if (this->state == RECORDING)
    {
      DEBUG("Got one buffer with %d bytes audio data", buffer.size);
      size_t written = this->converterStream->write(&buffer.data[0], buffer.size);
      if (written != buffer.size)
      {
        WARN("Could only write %d instead of %d bytes", written, buffer.size);
      }
    }

    ret = xQueueReceive(this->audioBuffersHandle, &buffer, 0);
  }
}

void VoiceAssistant::processAudioData()
{
  if (this->started)
  {
    int available = this->source->available();
    if (available >= AUDIO_BUFFER_SIZE)
    {
      AudioBuffer buffer;
      size_t read = this->source->readBytes(&(buffer.data[0]), AUDIO_BUFFER_SIZE);
      if (read > 0)
      {
        // Do something with the audio data, e.g. send it to voice assistant
        buffer.size = read;
        int ret = xQueueSend(this->audioBuffersHandle, (void *)&buffer, 0);
        if (ret == pdTRUE)
        {
          // No problem here
          if (this->recordingerror)
          {
            INFO("Recovered from recording error!");
          }
          this->recordingerror = false;
        }
        else if (ret == errQUEUE_FULL)
        {
          if (this->recordingerror == false)
          {
            this->recordingerror = true;
            WARN("No more place in audio buffers! Check is there is a network problem. I will restart the connection.");
            // this->connectOrReconnect(); TODO: How to restart everything properly??
          }
        }
      }
    }
  }
}

void VoiceAssistant::loop()
{
  if (this->started)
  {
    std::lock_guard<std::mutex> lck(this->loopmutex);
    this->webSocket->loop();
  }
}
