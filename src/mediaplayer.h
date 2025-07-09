#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include "sdmediaplayersource.h"

#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <mutex>

class MediaPlayer : public AudioPlayer {
private:
	SDMediaPlayerSource *source;

	MP3DecoderHelix *overrideHelix;
	FormatConverterStream *overrideFormatConverter;
	EncodedAudioStream *overrideDecoder;
	URLStream *overrideStream;
	StreamCopy overrideCopy;

	long lastoverridecopytime;
	int indexBeforeOverride;

	char *currentpath;

public:
	MediaPlayer(SDMediaPlayerSource &source, Print &output, AudioDecoder &decoder);

	virtual bool setVolume(float volume) override;

	const char *currentSong();

	int playProgressInPercent();

	void playURL(String url, bool forceMono);

	virtual size_t copy();

	bool hasPrevious();

	void playFromSD(String path, int index);

	char *getCurrentPath();
};

#endif
