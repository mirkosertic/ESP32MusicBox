#include <Arduino.h>

String LoggingFormatString(const char *format, ...);

#define LOGGING_ENABLED true

// #define DEBUG_LOGGING_ENABLED

#ifdef LOGGING_ENABLED
#define INFO(msg) Serial.printf("[INFO] %d %d %s:%s():%d - %s\n", millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, msg)
#define INFO_VAR(msg, ...) Serial.printf("[INFO] %d %d %s:%s():%d - %s\n", millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, LoggingFormatString(msg, __VA_ARGS__).c_str())

#define WARN(msg) Serial.printf("[WARN] %d %d %s:%s():%d - %s\n", millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, msg)
#define WARN_VAR(msg, ...) Serial.printf("[WARN] %d %d %s:%s():%d - %s\n", millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, LoggingFormatString(msg, __VA_ARGS__).c_str())
#endif

#ifdef DEBUG_LOGGING_ENABLED
#define DEBUG(msg) Serial.printf("[DEBUG] %d %d %s:%s():%d - %s\n", millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, msg)
#define DEBUG_VAR(msg, ...) Serial.printf("[DEBUG] %d %d %s:%s():%d - %s\n", millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, LoggingFormatString(msg, __VA_ARGS__).c_str())
#endif

#ifndef LOGGING_ENABLED
#define INFO(msg)
#define INFO_VAR(msg, ...)
#define WARN(msg)
#define WARN_VAR(msg, ...)
#endif

#ifndef DEBUG_LOGGING_ENABLED
#define DEBUG(msg)
#define DEBUG_VAR(msg, ...)
#endif