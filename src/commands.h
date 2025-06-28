#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>

const uint8_t COMMAND_VERSION = 10;
const uint8_t COMMAND_PLAY_DIRECTORY = 20;

struct CommandData // 44 bytes user data for a tag
{
	uint8_t version;
	uint8_t command;
	uint8_t volume;
	uint8_t index;
	uint8_t path[40];
};

#endif
