#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>

#include <mutex>

#include "mediaplayersource.h"

class MediaPlayer : public AudioPlayer
{
private:
    MediaPlayerSource *source;
    AudioStream *output;

    MP3DecoderHelix *overrideHelix;
    FormatConverterStream *overrideFormatConverter;
    EncodedAudioStream *overrideDecoder;
    URLStream *overrideStream;
    StreamCopy overrideCopy;

    long lastoverridecopytime;
    int indexBeforeOverride;

public:
    MediaPlayer(MediaPlayerSource &source, AudioStream &output, AudioDecoder &decoder);

    virtual bool next(int offset = 1) override;

    virtual bool previous(int offset = 1) override;

    virtual void setActive(bool isActive) override;

    virtual bool setVolume(float volume) override;

    const char *currentSong();

    int playProgressInPercent();

    void playURL(String url, bool forceMono);

    virtual size_t copy() override;
};

#endif