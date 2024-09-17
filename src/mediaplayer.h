#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <AudioTools.h>

typedef std::function<void()> ChangeNotifierCallback;

class MediaPlayer : public AudioPlayer
{
private:
    ChangeNotifierCallback changecallback;

public:
    MediaPlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder);

    void setChangCallback(ChangeNotifierCallback callback);

    virtual void setActive(bool isActive) override;

    virtual bool setVolume(float volume) override;
};

#endif