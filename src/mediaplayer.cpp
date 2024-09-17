#include "mediaplayer.h"

MediaPlayer::MediaPlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder)
    : AudioPlayer(source, output, decoder)
{
}

void MediaPlayer::setChangCallback(ChangeNotifierCallback callback)
{
    this->changecallback = callback;
}

void MediaPlayer::setActive(bool isActive)
{
    AudioPlayer::setActive(isActive);
    this->changecallback();
}

bool MediaPlayer::setVolume(float volume)
{
    bool result = AudioPlayer::setVolume(volume);
    this->changecallback();
    return result;
}
