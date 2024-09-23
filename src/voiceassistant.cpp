#include "voiceassistant.h"

#include <ArduinoJson.h>

#include "logging.h"

void vadispatcher(void *parameters)
{
  VoiceAssistant *target = (VoiceAssistant *)parameters;
  INFO("Voice assistant polling task started");
  while (true)
  {
    target->pollQueue();
    delay(5);
  }
}

void wsdispatcher(void *parameters)
{
  VoiceAssistant *target = (VoiceAssistant *)parameters;
  INFO("WebSocket looper task started");
  while (true)
  {
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

  int availableForWrite() { return 4096; }

  /// amount of data available
  int available()
  {
    return 0;
  }

  size_t write(const uint8_t *buffer, size_t size) override
  {
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
    break;
  }
  case WStype_CONNECTED:
  {
    INFO_VAR("Connected to url: %s", payload);
    this->stateIs(CONNECTED);
    break;
  }
  case WStype_TEXT:
  {

    INFO_VAR("Event Debug: %s", payload);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      WARN_VAR("Failed to parse: %s with %s", payload, error.f_str());
    }
    else
    {
      const char *type = doc["type"];
      INFO_VAR("Got event of type: %s", type);

      if (strcmp(type, "auth_required") == 0)
      {
        INFO("Sending authentication token");
        sendAuthentication();
      }
      if (strcmp(type, "auth_ok") == 0)
      {
        const char *haversion = doc["ha_version"];
        INFO_VAR("Authenticated against Homeassistant %s", haversion);
        this->stateIs(AUTHENTICATED);
      }
      if (strcmp(type, "auth_invalid") == 0)
      {
        const char *message = doc["message"];
        WARN_VAR("Authenticated failure : %s", message);
        this->stateIs(AUTHENTICATIONERROR);
      }
      if (strcmp(type, "event") == 0)
      {
        if (strcmp(doc["event"]["type"], "run-start") == 0)
        {
          this->binaryHandler = doc["event"]["data"]["runner_data"]["stt_binary_handler_id"];
          INFO_VAR("run-start received with binary handler id %d", this->binaryHandler);
          this->stateIs(STARTED);
        }
        if (strcmp(doc["event"]["type"], "run-end") == 0)
        {
          INFO("run-end received");

          if (this->urlToSpeak.length() > 0)
          {
            INFO_VAR("Playing URL %s", this->urlToSpeak.c_str());

            /*            if (!m_speaker->playAudioFromURL(m_urlToSpeak.c_str()))
                        {
                          WARN("Failed to start playback.");
                        }*/
          }

          this->stateIs(FINISHED);
        }
        if (strcmp(doc["event"]["type"], "wake_word-start") == 0)
        {
          int sampleRate = doc["event"]["data"]["metadata"]["sample_rate"];
          uint16_t channelCount = doc["event"]["data"]["metadata"]["channel"];
          uint8_t bitsPerSample = doc["event"]["data"]["metadata"]["bit_rate"];

          AudioInfo from = this->source->audioInfoOut();
          AudioInfo to;
          to.copyFrom(from);
          to.bits_per_sample = bitsPerSample;
          to.channels = channelCount;
          to.sample_rate = sampleRate;

          INFO_VAR("Source is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);
          INFO_VAR("Needs to be resampled to Samplerate=%d, Channels=%d and Bits per sample=%d", to.sample_rate, to.channels, to.bits_per_sample);

          // TODO
          this->converterstream->begin(from, to);

          INFO("wake_word-start received");
          this->stateIs(RECORDING);
        }
        if (strcmp(doc["event"]["type"], "stt-start") == 0)
        {
          int sampleRate = doc["event"]["data"]["metadata"]["sample_rate"];
          uint16_t channelCount = doc["event"]["data"]["metadata"]["channel"];
          uint8_t bitsPerSample = doc["event"]["data"]["metadata"]["bit_rate"];

          AudioInfo from = this->source->audioInfoOut();
          AudioInfo to;
          to.copyFrom(from);
          to.bits_per_sample = bitsPerSample;
          to.channels = channelCount;
          to.sample_rate = sampleRate;

          INFO_VAR("Source is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);
          INFO_VAR("Needs to be resampled to Samplerate=%d, Channels=%d and Bits per sample=%d", to.sample_rate, to.channels, to.bits_per_sample);

          // TODO
          this->converterstream->begin(from, to);

          INFO("stt-start received");
          this->stateIs(RECORDING);
          // m_speaker->playReady();
        }
        if (strcmp(doc["event"]["type"], "stt-vad-end") == 0)
        {
          INFO("wstt-vad-end received");
          this->stateIs(STTFINISHED);

          finishAudioStream();
        }
        if (strcmp(doc["event"]["type"], "tts-end") == 0)
        {
          const char *ressource = doc["event"]["data"]["tts_output"]["url"];
          this->urlToSpeak = String("http://192.168.0.159:8080");
          this->urlToSpeak += &(ressource[4]);

          INFO_VAR("tts-end received. Playing URL %s", this->urlToSpeak.c_str());
        }
      }
    }
    break;
  }
  case WStype_BIN:
    INFO_VAR("Get binary length: %u", length);
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

VoiceAssistant::VoiceAssistant(AudioStream *source)
{
  this->audioBuffersHandle = xQueueCreate(16, sizeof(AudioBuffer));
  if (audioBuffersHandle == NULL)
  {
    WARN("Audio buffers queue could not be created. Halt.");
    while (1)
    {
      delay(1000);
    }
  }

  this->state = DISCONNECTED;
  this->source = source;
  this->commandid = 1;
  this->webSocket = new WebSocketsClient();
  this->outputdelegate = new VoiceAssistantStream(this);
  this->converterstream = new FormatConverterStream(*this->outputdelegate);
  this->started = false;

  this->webSocket->setReconnectInterval(1000);
  this->webSocket->onEvent([this](WStype_t type, uint8_t *payload, size_t length)
                           { this->webSocketEvent(type, payload, length); });
}

VoiceAssistant::~VoiceAssistant()
{
  this->webSocket->disconnect();
  delete this->webSocket;
  delete this->outputdelegate;
  delete this->converterstream;
}

void VoiceAssistant::begin(String host, int port, String token, StateNotifierCallback stateNotifier)
{
  INFO_VAR("Connecting to WebSocket %s:%d", host.c_str(), port);
  this->token = token;
  this->stateNotifier = stateNotifier;

  this->webSocket->begin(host, port, "/api/websocket");

  xTaskCreate(vadispatcher, "Voice Assistant", 8192, this, 2, NULL);
  xTaskCreate(wsdispatcher, "WebSocket", 8192, this, 2, NULL);

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

    AudioInfo info = this->source->audioInfoOut();
    INFO_VAR("Got AudioInfo with Samplerate=%d, Channels=%d and Bits per sample=%d", info.sample_rate, info.channels, info.bits_per_sample);

    runcmd["end_stage"] = "tts";
    runcmd["input"]["sample_rate"] = info.sample_rate;
    runcmd["input"]["volume_multiplier"] = 1.5;

    String buffer;
    size_t buffersize = serializeJson(runcmd, buffer);

    INFO_VAR("Debug payload %s", buffer.c_str());

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
  // INFO_VAR("Sending %d bytes audio data", length);

  int transferlength = 1 + length;

  uint8_t transferbuffer[transferlength];
  transferbuffer[0] = this->binaryHandler;

  memcpy(&transferbuffer[1], data, length);

  this->webSocket->sendBIN(transferbuffer, transferlength);
}

void VoiceAssistant::finishAudioStream()
{
  uint8_t transferbuffer[1];
  transferbuffer[0] = this->binaryHandler;

  this->webSocket->sendBIN(&transferbuffer[0], 1);
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
      // INFO_VAR("Got one buffer with %d bytes audio data", buffer.size);
      this->converterstream->write(&buffer.data[0], buffer.size);
    }

    vTaskDelay(1);

    ret = xQueueReceive(this->audioBuffersHandle, &buffer, 0);
  }
}

void VoiceAssistant::processAudioData(const AudioBuffer *data)
{
  if (this->started)
  {
    int ret = xQueueSend(this->audioBuffersHandle, (void *)data, 0);
    if (ret == pdTRUE)
    {
      // No problem here
    }
    else if (ret == errQUEUE_FULL)
    {
      WARN("No more place in audio buffers!");
    }
  }
}

int VoiceAssistant::getRecordingBlockSize()
{
  return AUDIO_BUFFER_SIZE;
}

void VoiceAssistant::loop()
{
  if (this->started)
  {
    this->webSocket->loop();
  }
}
