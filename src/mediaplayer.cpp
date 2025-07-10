#include "mediaplayer.h"

#include "logging.h"

MediaPlayer::MediaPlayer(SDMediaPlayerSource &sourceSD, URLMediaPlayerSource &sourceURL, Print &output, AudioDecoder &decoder)
	: AudioPlayer(sourceSD, output, decoder) {
	this->sourceSD = &sourceSD;
	this->sourceURL = &sourceURL;
	this->currentpath = new char[512];

	this->overrideHelix = new MP3DecoderHelix();
	this->overrideHelix->driver()->setInfoCallback([](MP3FrameInfo &info, void *ref) { INFO("Got libHelix Info %d bitrate %d bits per sample %d channels", info.bitrate, info.bitsPerSample, info.nChans); }, nullptr);

	this->overrideFormatConverter = new FormatConverterStream(output);
	this->overrideDecoder = new EncodedAudioStream(this->overrideFormatConverter, this->overrideHelix);
	this->overrideHelix->addNotifyAudioChange(*this->overrideDecoder);
	this->overrideDecoder->addNotifyAudioChange(*this->overrideFormatConverter);
	this->overrideStream = nullptr;
	this->lastoverridecopytime = -1;
}

bool MediaPlayer::setVolume(float volume) {
	return AudioPlayer::setVolume(volume);
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
	this->begin(index, true);
}

char *MediaPlayer::getCurrentPath() {
	return this->currentpath;
}

void MediaPlayer::playURL(String url, bool forceMono) {
	/*    INFO("Playing URL %s", url.c_str());

		if (this->overrideStream != nullptr)
		{
			this->overrideStream->end();
			delete this->overrideStream;
		}
		this->overrideStream = new URLStream(2024);
		if (!this->overrideStream->begin(url.c_str()))
		{
			WARN("Could not begin stream from %s", url.c_str());
		}
		this->overrideDecoder->begin();

		AudioInfo outputInfo = this->output->audioInfo();
		DEBUG("Begin of Helix");
		this->overrideHelix->begin();
		DEBUG("Begin of decoder");
		this->overrideDecoder->begin();
		DEBUG("Begin of converter");
		if (forceMono)
		{
			this->overrideFormatConverter->begin(AudioInfo(1, 1, 16), outputInfo);
		}
		else
		{
			this->overrideFormatConverter->begin(outputInfo, outputInfo);
		}
		DEBUG("Start of copy");
		this->overrideCopy.begin(*this->overrideDecoder, *this->overrideStream);
		this->lastoverridecopytime = -1;*/
}

const char *MediaPlayer::currentSong() {
	return this->sourceSD->currentPlayFile();
}

int MediaPlayer::playProgressInPercent() {
	return this->sourceSD->playProgressInPercent();
}
