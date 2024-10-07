#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>

#include <mutex>

#include "mediaplayersource.h"

typedef std::function<void(bool active, float volume, const char *currentsong)> ChangeNotifierCallback;

class MediaPlayer : public AudioPlayer
{
private:
    MediaPlayerSource *source;
    AudioStream *output;

    ChangeNotifierCallback changecallback;

    MP3DecoderHelix *overrideHelix;
    FormatConverterStream *overrideFormatConverter;
    EncodedAudioStream *overrideDecoder;
    URLStream *overrideStream;
    StreamCopy overrideCopy;

    long lastoverridecopytime;
    int indexBeforeOverride;

public:
    MediaPlayer(MediaPlayerSource &source, AudioStream &output, AudioDecoder &decoder);

    void setChangCallback(ChangeNotifierCallback callback);

    virtual bool next(int offset = 1) override;

    virtual bool previous(int offset = 1) override;

    virtual void setActive(bool isActive) override;

    virtual bool setVolume(float volume) override;

    void playURL(String url, bool forceMono);

    virtual size_t copy() override;
};

#endif