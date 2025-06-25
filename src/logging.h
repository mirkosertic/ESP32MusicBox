#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>

#define LOGGING_ENABLED

#ifdef LOGGING_ENABLED
#define INFO(msg, ...) Serial.printf("[INFO] %lu %d %s#%d:%s() - " msg "\n", millis(), xPortGetCoreID(), __FILE__, __LINE__, __func__, ##__VA_ARGS__);
#define WARN(msg, ...) Serial.printf("[WARN] %lu %d %s#%d:%s() - " msg "\n", millis(), xPortGetCoreID(), __FILE__, __LINE__, __func__, ##__VA_ARGS__);
#endif

#ifdef DEBUG_LOGGING_ENABLED
#define DEBUG(msg, ...) Serial.printf("[DEBUG] %lu %d %s#%d:%s() - " msg "\n", millis(), xPortGetCoreID(), __FILE__, __LINE__, __func__, ##__VA_ARGS__);
#endif

#ifndef LOGGING_ENABLED
#define INFO(msg, ...)
#define WARN(msg, ...)
#endif

#ifndef DEBUG_LOGGING_ENABLED
#define DEBUG(msg, ...)
#endif

#endif