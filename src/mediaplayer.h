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

    MP3DecoderHelix *overrideHelix;
    FormatConverterStream *overrideFormatConverter;
    EncodedAudioStream *overrideDecoder;
    URLStream *overrideStream;
    StreamCopy overrideCopy;

    long lastoverridecopytime;
    int indexBeforeOverride;

public:
    MediaPlayer(MediaPlayerSource &source, Print &output, AudioDecoder &decoder);

    virtual bool setVolume(float volume) override;

    const char *currentSong();

    int playProgressInPercent();

    void playURL(String url, bool forceMono);

    virtual size_t copy();
};

#endif