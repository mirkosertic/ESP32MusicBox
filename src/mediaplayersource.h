#ifndef MEDIAPLAYERSOURCE_H
#define MEDIAPLAYERSOURCE_H

class MediaPlayerSource {
private:
public:
	virtual int playProgressInPercent() = 0;

	virtual const char *currentPlayFile() = 0;
};

#endif
