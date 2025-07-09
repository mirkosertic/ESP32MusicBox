#ifndef SDMEDIAPLAYERSOURCE_H
#define SDMEDIAPLAYERSOURCE_H

#include "mediaplayersource.h"

#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSD.h>

typedef std::function<void(Stream *)> ChangeIndexCallback;

class SDMediaPlayerSource : public MediaPlayerSource, public AudioSourceSD {
private:
	ChangeIndexCallback changeindexcallback;
	Stream *currentStream;

public:
	SDMediaPlayerSource(const char *startFilePath = "/", const char *ext = ".mp3", bool setupIndex = true);

	void setChangeIndexCallback(ChangeIndexCallback callback);

	virtual Stream *selectStream(int index) override;

	virtual int playProgressInPercent() override;

	virtual const char *currentPlayFile() override;
};

#endif
