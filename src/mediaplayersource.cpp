#include "mediaplayersource.h"

#include "logging.h"

MediaPlayerSource::MediaPlayerSource(const char *startFilePath, const char *ext, bool setupIndex)
    //: AudioSourceSD(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS, setupIndex)
    : AudioSourceSDMMC(startFilePath, ext, setupIndex)
{
}

void MediaPlayerSource::setChangeIndexCallback(ChangeIndexCallback callback)
{
    this->changeindexcallback = callback;
}

Stream *MediaPlayerSource::selectStream(int index)
{
    INFO_VAR("Selecting next stream #%d", index);
    this->currentStream = AudioSourceSDMMC::selectStream(index);
    this->changeindexcallback(this->currentStream);
    return this->currentStream;
}
