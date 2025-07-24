#include "sdmediaplayersource.h"

#include "logging.h"

SDMediaPlayerSource::SDMediaPlayerSource(PlaystateMonitor *monitor, const char *startFilePath, const char *ext, bool setupIndex)
	: AudioSourceSD(startFilePath, ext, setupIndex) {
	this->is_sd_setup = true;
	this->currentStream = NULL;
	this->monitor = monitor;
}

void SDMediaPlayerSource::setChangeIndexCallback(ChangeIndexCallback callback) {
	this->changeindexcallback = callback;
}

Stream *SDMediaPlayerSource::selectStream(int index) {
	INFO("Selecting next stream #%d, current path is %s", index, start_path);
	this->currentStream = AudioSourceSD::selectStream(index);
	this->changeindexcallback(this->currentStream);
	if (this->currentStream != NULL) {
		INFO("Got a new stream for this index!");
		this->monitor->markPlayState(String(this->start_path), index);
	}
	return this->currentStream;
}

const char *SDMediaPlayerSource::currentPlayFile() {
	if (this->currentStream == NULL) {
		return NULL;
	}
	File *file = (File *) (this->currentStream);
	return file->name();
}

int SDMediaPlayerSource::playProgressInPercent() {
	if (this->currentStream == NULL) {
		return 0;
	}
	File *file = (File *) (this->currentStream);
	return (int) (((float) file->position()) / file->size() * 100.0);
}
