#ifndef URLMEDIAPLAYERSOURCE_H
#define URLMEDIAPLAYERSOURCE_H

#include "mediaplayersource.h"

#include <AudioTools/Disk/AudioSourceURL.h>

class URLMediaPlayerSource : public MediaPlayerSource, public AudioSourceDynamicURL {
public:
	URLMediaPlayerSource(AbstractURLStream &urlStream, const char *mime, int startPos = 0);

	virtual const char *value(int pos) override;

	virtual int playProgressInPercent() override;

	virtual const char *currentPlayFile() override;
};
#endif
