#include "mediaplayer.h"

#include "logging.h"

MediaPlayer::MediaPlayer(SDMediaPlayerSource &sourceSD, URLMediaPlayerSource &sourceURL, Print &output, AudioDecoder &decoder)
	: AudioPlayer(sourceSD, output, decoder) {
	this->sourceSD = &sourceSD;
	this->sourceURL = &sourceURL;
	this->currentpath = new char[512];
}

bool MediaPlayer::hasPrevious() {
	return this->sourceSD->index() > 0;
}

void MediaPlayer::playFromSD(String path, int index) {
	INFO("Playing song in path %s with index %d", path.c_str(), index);
	strcpy(this->currentpath, path.c_str());

	INFO("Player active=false");
	this->setActive(false);
	INFO("Setting path");
	this->sourceSD->setPath(currentpath);
	INFO("Playing index %d", index);
	this->setAudioSource(*this->sourceSD);
	this->begin(index, true);

	this->resetPlayDirection();
}

char *MediaPlayer::getCurrentPath() {
	return this->currentpath;
}

void MediaPlayer::playURL(String url, bool forceMono) {
	INFO("Playing URL %s", url.c_str());
	INFO("Player active=false");
	this->setActive(false);
	this->setAudioSource(*this->sourceURL);
	INFO("Adding URL");
	this->sourceURL->clear();
	this->sourceURL->addURL(url.c_str());
	INFO("Starting playback");
	this->begin(0, true);
}

const char *MediaPlayer::currentSong() {
	return this->sourceSD->currentPlayFile();
}

int MediaPlayer::playProgressInPercent() {
	return this->sourceSD->playProgressInPercent();
}

void MediaPlayer::resetPlayDirection() {
	this->stream_increment = 1;
	INFO("Stream increment set to %d", stream_increment);
}
