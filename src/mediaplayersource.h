#ifndef MEDIAPLAYERSOURCE_H
#define MEDIAPLAYERSOURCE_H

#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSD.h>

typedef std::function<void(Stream *)> ChangeIndexCallback;

class MediaPlayerSource : public AudioSourceSD {
private:
	ChangeIndexCallback changeindexcallback;
	Stream *currentStream;

public:
	MediaPlayerSource(const char *startFilePath = "/", const char *ext = ".mp3", bool setupIndex = true);

	void setChangeIndexCallback(ChangeIndexCallback callback);

	virtual Stream *selectStream(int index) override;

	int playProgressInPercent();

	const char *currentPlayFile();
};

#endif
