#include "mediaplayersource.h"

#include "logging.h"

MediaPlayerSource::MediaPlayerSource(const char *startFilePath, const char *ext, bool setupIndex)
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

int MediaPlayerSource::playProgressInPercent()
{
    if (this->currentStream == NULL)
    {
        return 0;
    }
    File *file = (File *)(this->currentStream);
    return (int)(((float)file->position()) / file->size() * 100.0);
}