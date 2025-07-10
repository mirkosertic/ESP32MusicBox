#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include "sdmediaplayersource.h"
#include "urlmediaplayersource.h"

#include <AudioTools.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <mutex>

class MediaPlayer : public AudioPlayer {
private:
	SDMediaPlayerSource *sourceSD;
	URLMediaPlayerSource *sourceURL;

	MP3DecoderHelix *overrideHelix;
	FormatConverterStream *overrideFormatConverter;
	EncodedAudioStream *overrideDecoder;
	URLStream *overrideStream;
	StreamCopy overrideCopy;

	long lastoverridecopytime;
	int indexBeforeOverride;

	char *currentpath;

public:
	MediaPlayer(SDMediaPlayerSource &sourceSD, URLMediaPlayerSource &sourceURL, Print &output, AudioDecoder &decoder);

	virtual bool setVolume(float volume) override;

	const char *currentSong();

	int playProgressInPercent();

	void playURL(String url, bool forceMono);

	bool hasPrevious();

	void playFromSD(String path, int index);

	char *getCurrentPath();
};

#endif
