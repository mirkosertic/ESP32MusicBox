#include "mediaplayer.h"

#include "logging.h"

MediaPlayer::MediaPlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder)
    : AudioPlayer(source, output, decoder)
{
    this->output = &output;

    this->overrideHelix = new MP3DecoderHelix();
    this->overrideHelix->driver()->setInfoCallback([](MP3FrameInfo &info, void *ref)
                                                   { INFO_VAR("Got libHelix Info %d bitrate %d bits per sample %d channels", info.bitrate, info.bitsPerSample, info.nChans); }, nullptr);

    this->overrideFormatConverter = new FormatConverterStream(output);
    this->overrideDecoder = new EncodedAudioStream(this->overrideFormatConverter, this->overrideHelix);
    this->overrideHelix->addNotifyAudioChange(*this->overrideDecoder);
    this->overrideDecoder->addNotifyAudioChange(*this->overrideFormatConverter);
    this->overrideStream = nullptr;
    this->lastoverridecopytime = -1;
}

void MediaPlayer::setChangCallback(ChangeNotifierCallback callback)
{
    this->changecallback = callback;
}

void MediaPlayer::setActive(bool isActive)
{
    {
        std::lock_guard<std::mutex> lck(this->loopmutex);
        AudioPlayer::setActive(isActive);
    }
    this->changecallback();
}

bool MediaPlayer::setVolume(float volume)
{
    bool result;
    {
        std::lock_guard<std::mutex> lck(this->loopmutex);
        result = AudioPlayer::setVolume(volume);
    }
    this->changecallback();
    return result;
}

void MediaPlayer::playURL(String url)
{
    INFO_VAR("Playing URL %s", url.c_str());
    std::lock_guard<std::mutex> lck(this->loopmutex);

    if (this->overrideStream != nullptr)
    {
        this->overrideStream->end();
        delete this->overrideStream;
    }
    this->overrideStream = new URLStream(2024);
    if (!this->overrideStream->begin(url.c_str()))
    {
        WARN_VAR("Could not begin stream from %s", url.c_str());
    }
    this->overrideDecoder->begin();

    AudioInfo outputInfo = this->output->audioInfo();
    DEBUG("Begin of Helix");
    this->overrideHelix->begin();
    DEBUG("Begin of decoder");
    this->overrideDecoder->begin();
    DEBUG("Begin of converter");
    this->overrideFormatConverter->begin(AudioInfo(1, 1, 16), outputInfo);
    DEBUG("Start of copy");
    this->overrideCopy.begin(*this->overrideDecoder, *this->overrideStream);
    this->lastoverridecopytime = -1;
}

size_t MediaPlayer::copy()
{
    std::lock_guard<std::mutex> lck(this->loopmutex);

    if (this->overrideStream == nullptr)
    {
        return AudioPlayer::copy();
    }

    AudioInfo outinfo = this->output->audioInfo();
    DEBUG_VAR("Player is running with Samplerate=%d, Channels=%d and Bits per sample=%d", outinfo.sample_rate, outinfo.channels, outinfo.bits_per_sample);

    AudioInfo from = this->overrideDecoder->audioInfo();
    DEBUG_VAR("Decoder is running with Samplerate=%d, Channels=%d and Bits per sample=%d", from.sample_rate, from.channels, from.bits_per_sample);
    AudioInfo out = this->overrideDecoder->audioInfoOut();
    DEBUG_VAR("Decoder out is running with Samplerate=%d, Channels=%d and Bits per sample=%d", out.sample_rate, out.channels, out.bits_per_sample);

    AudioInfo conv = this->overrideFormatConverter->audioInfo();
    DEBUG_VAR("Conv is running with Samplerate=%d, Channels=%d and Bits per sample=%d", conv.sample_rate, conv.channels, conv.bits_per_sample);
    AudioInfo convout = this->overrideFormatConverter->audioInfoOut();
    DEBUG_VAR("Conv out is running with Samplerate=%d, Channels=%d and Bits per sample=%d", convout.sample_rate, convout.channels, convout.bits_per_sample);

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
    return result;
}