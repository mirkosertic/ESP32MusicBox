#include "mediaplayer.h"

#include "logging.h"

MediaPlayer::MediaPlayer(SDMediaPlayerSource &source, Print &output, AudioDecoder &decoder)
	: AudioPlayer(source, output, decoder) {
	this->source = &source;
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
	return this->source->index() > 0;
}

void MediaPlayer::playFromSD(String path, int index) {
	INFO("Playing song in path %s with index %d", path.c_str(), index);
	strcpy(this->currentpath, path.c_str());

	INFO("Player active=false");
	this->setActive(false);
	INFO("Setting path");
	this->source->setPath(currentpath);
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

size_t MediaPlayer::copy() {
	// if (this->overrideStream == nullptr)
	//{
	return AudioPlayer::copy();
	//}
	/*
		AudioInfo outinfo = this->output->audioInfo();
		DEBUG("Player is running with Samplerate=%d, Channels=%d and Bits per sample=%d", outinfo.sample_rate, outinfo.channels, outinfo.bits_per_sample);

		AudioInfo from = this->overrideDecoder->audioInfo();
		DEBUG("Decoder is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);
		AudioInfo out = this->overrideDecoder->audioInfoOut();
		DEBUG("Decoder out is running with Samplerate=%d, Channels=%d and Bits per sample=%d", out.sample_rate, out.channels, out.bits_per_sample);

		AudioInfo conv = this->overrideFormatConverter->audioInfo();
		DEBUG("Conv is running with Samplerate=%d, Channels=%d and Bits per sample=%d", conv.sample_rate, conv.channels, conv.bits_per_sample);
		AudioInfo convout = this->overrideFormatConverter->audioInfoOut();
		DEBUG("Conv out is running with Samplerate=%d, Channels=%d and Bits per sample=%d", convout.sample_rate, convout.channels, convout.bits_per_sample);

		size_t result = this->overrideCopy.copy();
		long now = millis();
		if (result > 0)
		{
			this->lastoverridecopytime = now;
		}
		else
		{
			if (lastoverridecopytime != 1 && now - this->lastoverridecopytime > 50)
			{
				// More than 50 milliseconds nothing copied, we assume playback has finished
				INFO("Override playback finished, switching back to regular playback...");
				this->overrideStream->end();
				delete this->overrideStream;
				this->overrideStream = nullptr;

				if (this->isActive())
				{
					// Beware of the not reentrant mutexes!
					INFO("Toggling active state");
					AudioPlayer::setActive(false);
					AudioPlayer::setActive(true);
				}

				INFO("Done");
			}
		}
		return result;*/
}

const char *MediaPlayer::currentSong() {
	return this->source->currentPlayFile();
}

int MediaPlayer::playProgressInPercent() {
	return this->source->playProgressInPercent();
}
