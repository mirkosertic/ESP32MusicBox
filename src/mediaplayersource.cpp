#include "mediaplayersource.h"

#include "logging.h"

MediaPlayerSource::MediaPlayerSource(const char *startFilePath, const char *ext, bool setupIndex)
	: AudioSourceSD(startFilePath, ext, setupIndex) {
	this->is_sd_setup = true;
	this->currentStream = NULL;
}

void MediaPlayerSource::setChangeIndexCallback(ChangeIndexCallback callback) {
	this->changeindexcallback = callback;
}

Stream *MediaPlayerSource::selectStream(int index) {
	INFO("Selecting next stream #%d", index);
	this->currentStream = AudioSourceSD::selectStream(index);
	this->changeindexcallback(this->currentStream);
	return this->currentStream;
}

const char *MediaPlayerSource::currentPlayFile() {
	if (this->currentStream == NULL) {
		return NULL;
	}
	File *file = (File *) (this->currentStream);
	return file->name();
}

int MediaPlayerSource::playProgressInPercent() {
	if (this->currentStream == NULL) {
		return 0;
	}
	File *file = (File *) (this->currentStream);
	return (int) (((float) file->position()) / file->size() * 100.0);
}
