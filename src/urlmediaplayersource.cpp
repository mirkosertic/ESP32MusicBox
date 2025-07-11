#include "urlmediaplayersource.h"

#include "logging.h"

URLMediaPlayerSource::URLMediaPlayerSource(AbstractURLStream &urlStream, const char *mime, int startPos)
	: AudioSourceDynamicURL(urlStream, mime, startPos) {
}

const char *URLMediaPlayerSource::value(int pos) {
	INFO("Resolving URL with index %d", pos);
	return AudioSourceDynamicURL::value(pos);
}

int URLMediaPlayerSource::playProgressInPercent() {
	return 0;
}

const char *URLMediaPlayerSource::currentPlayFile() {
	return this->toStr();
}
