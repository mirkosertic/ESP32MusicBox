#ifndef MEDIAPLAYERSOURCE_H
#define MEDIAPLAYERSOURCE_H

#include <AudioTools.h>
//#include <AudioLibs/AudioSourceSDMMC.h>
#include <AudioLibs/AudioSourceSD.h>

typedef std::function<void(Stream*)> ChangeIndexCallback;

class MediaPlayerSource : public AudioSourceSD
{
private:
    ChangeIndexCallback changeindexcallback;

public:
    MediaPlayerSource(const char *startFilePath = "/", const char *ext = ".mp3", int cspin = 5, bool setupIndex = true);

    void setChangeIndexCallback(ChangeIndexCallback callback);

    virtual Stream *selectStream(int index) override;
};

#endif