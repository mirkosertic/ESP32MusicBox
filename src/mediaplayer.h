#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <AudioTools.h>
#include "AudioCodecs/CodecMP3Helix.h"

#include <mutex>

typedef std::function<void()> ChangeNotifierCallback;

class MediaPlayer : public AudioPlayer
{
private:
    AudioStream *output;

    ChangeNotifierCallback changecallback;

    MP3DecoderHelix *overrideHelix;
    FormatConverterStream *overrideFormatConverter;
    EncodedAudioStream *overrideDecoder;
    URLStream *overrideStream;
    StreamCopy overrideCopy;

    long lastoverridecopytime;
    int indexBeforeOverride;

    std::mutex loopmutex;

public:
    MediaPlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder);

    void setChangCallback(ChangeNotifierCallback callback);

    virtual void setActive(bool isActive) override;

    virtual bool setVolume(float volume) override;

    void playURL(String url);

    virtual size_t copy() override;
};

#endif