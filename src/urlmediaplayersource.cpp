#include "urlmediaplayersource.h"

URLMediaPlayerSource::URLMediaPlayerSource(AbstractURLStream &urlStream, const char *mime, int startPos)
	: AudioSourceDynamicURL(urlStream, mime, startPos) {
}

int URLMediaPlayerSource::playProgressInPercent() {
	return 0;
}

const char *URLMediaPlayerSource::currentPlayFile() {
	return this->toStr();
}
