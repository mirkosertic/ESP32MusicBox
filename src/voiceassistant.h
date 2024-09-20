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

class VoiceAssistant
{
private:
    WebSocketsClient *webSocket;
    HAState state;
    uint8_t binaryHandler;
    String urlToSpeak;
    int commandid;

    String token;

    StateNotifierCallback stateNotifier;

    AudioStream *source;
    AudioStream *outputdelegate;
    FormatConverterStream *converterstream;

    void finishAudioStream();

    void stateIs(HAState state);

public:
    VoiceAssistant(AudioStream *source);
    ~VoiceAssistant();

    void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

    void begin(String host, int port, String token, StateNotifierCallback stateNotifier);

    void sendAuthentication();

    bool startPipeline(bool includeWakeWordDetection);

    void loop();

    void reset();

    void checkForAudioData();

    void sendAudioData(const uint8_t *data, int length);

    HAState currentState();
};

#endif