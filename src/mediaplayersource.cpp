#include "mediaplayersource.h"

MediaPlayerSource::MediaPlayerSource(const char *startFilePath, const char *ext, int cspin, bool setupIndex)
    : AudioSourceSD(startFilePath, ext, cspin, setupIndex)
{
}

void MediaPlayerSource::setChangeIndexCallback(ChangeIndexCallback callback)
{
    this->changeindexcallback = callback;
}

Stream *MediaPlayerSource::selectStream(int index)
{
    Serial.println(String("selectStream() - Selecting next stream #") + index);
    Stream *result = AudioSourceSD::selectStream(index);

    this->changeindexcallback(result);

    return result;
}
