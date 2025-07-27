#ifndef PLAYSTATEMONITOR_H
#define PLAYSTATEMONITOR_H

#include <FS.h>

class PlaystateMonitor {
private:
	FS *fs;

public:
	PlaystateMonitor(FS *fs);

	void markPlayState(String directory, int index);
	int lastPlayindexFor(String directory, int defaultIndex);

	String lastPlaybackDirectory();
};

#endif
