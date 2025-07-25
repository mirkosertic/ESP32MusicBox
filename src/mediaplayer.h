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

	char *currentpath;

public:
	MediaPlayer(SDMediaPlayerSource &sourceSD, URLMediaPlayerSource &sourceURL, Print &output, AudioDecoder &decoder);

	const char *currentSong();

	int playProgressInPercent();

	void playURL(String url, bool forceMono);

	bool hasPrevious();

	void playFromSD(String path, int index);

	char *getCurrentPath();

	void resetPlayDirection();
};

#endif
