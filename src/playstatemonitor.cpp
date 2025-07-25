#include "playstatemonitor.h"

#include "logging.h"

#include <ArduinoJson.h>

#define PLAYSTATE_CONFIG_FILE "/playstate.json"

PlaystateMonitor::PlaystateMonitor(FS *fs) {
	this->fs = fs;
}

void PlaystateMonitor::markPlayState(String directory, int index) {
	// Try to read configuration file from fs...
	File configFile = this->fs->open(PLAYSTATE_CONFIG_FILE, FILE_READ);
	JsonDocument document;
	if (!configFile) {
		INFO("No playstate config file detected!");
	} else {
		DeserializationError error = deserializeJson(document, configFile);
		configFile.close();

		if (error) {
			WARN("Could not read playstate config file!");
			document = JsonDocument();
			// Create new file
		} else {
			// Everything fine and loade
		}
	}

	// We store the progress with the path as its key and the index as the value
	document[directory] = index;
	document['.lastused'] = directory;

	// Write it back to FS
	configFile = this->fs->open(PLAYSTATE_CONFIG_FILE, FILE_WRITE, true);
	if (!configFile) {
		WARN("Could not create playstate config file!");
	} else {
		serializeJson(document, configFile);
		configFile.close();
		INFO("Playstate config file updated for %s to index %d", directory.c_str(), index);
	}
}

int PlaystateMonitor::lastPlayindexFor(String directory, int defaultIndex) {
	// Try to read configuration file from fs...
	File configFile = this->fs->open(PLAYSTATE_CONFIG_FILE, FILE_READ);
	JsonDocument document;
	if (!configFile) {
		INFO("No playstate config file detected!");
		return defaultIndex;
	}
	DeserializationError error = deserializeJson(document, configFile);
	configFile.close();

	if (error) {
		WARN("Could not read playstate config file!");
		return defaultIndex;
	}

	// We store the progress with the path as its key and the index as the value
	if (document[directory].is<int>()) {
		int index = document[directory].as<int>();
		INFO("Last index for directory %s was %d", directory.c_str(), index);
		return index;
	}

	INFO("No last index found for directory %s. Starting with default %d", directory.c_str(), defaultIndex);
	return defaultIndex;
}
