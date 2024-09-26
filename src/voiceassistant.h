#ifndef VOICEASSISTANT_H
#define VOICEASSISTANT_H

#include <Arduino.h>
#include <AudioTools.h>
#include <WebSocketsClient.h>

enum HAState
{
    DISCONNECTED,
    CONNECTED,
    AUTHREQUESTED,
    AUTHENTICATED,
    AUTHENTICATIONERROR,
    STARTED,
    FINISHED,
    RECORDING,
    STTFINISHED
};

typedef std::function<void(HAState)> StateNotifierCallback;

typedef std::function<void(String)> PlayAudioFeedbackCallback;

#define AUDIO_BUFFER_SIZE 2048
typedef struct
{
    size_t size;
    uint8_t data[AUDIO_BUFFER_SIZE];
} AudioBuffer;

class VoiceAssistant
{
private:
    WebSocketsClient *webSocket;
    HAState state;
    uint8_t binaryHandler;
    String urlToSpeak;
    int commandid;

    String token;
    String deviceId;
    String baseUrl;

    bool started;

    StateNotifierCallback stateNotifier;
    PlayAudioFeedbackCallback playAudioFeedbackCallback;

    AudioInfo recordingQuality;

    AudioStream *source;
    AudioStream *outputdelegate;
    BufferedStream *buffer;
    FormatConverterStream *converterStream;

    QueueHandle_t audioBuffersHandle;

    void finishAudioStream();

    void stateIs(HAState state);

public:
    VoiceAssistant(AudioStream *source);
    ~VoiceAssistant();

    void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

    void begin(String host, int port, String token, String deviceId, StateNotifierCallback stateNotifier, PlayAudioFeedbackCallback playFeedbackCallback);

    void sendAuthentication();

    bool startPipeline(bool includeWakeWordDetection);

    void loop();

    void reset();

    int getRecordingBlockSize();

    void pollQueue();

    void sendAudioData(const uint8_t *data, size_t length);

    void processAudioData(const AudioBuffer *data);

    HAState currentState();
};

#endif