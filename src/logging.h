#include <Arduino.h>

#define LOGGING_ENABLED

// #define DEBUG_LOGGING_ENABLED

#ifdef LOGGING_ENABLED
#define INFO(msg) Serial.printf((String("[INFO] %d %d %s:%s():%d - ") + msg + "\n").c_str(), millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__);
#define INFO_VAR(msg, ...) Serial.printf((String("[INFO] %d %d %s:%s():%d - ") + msg + "\n").c_str(), millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, __VA_ARGS__);

#define WARN(msg) Serial.printf((String("[WARN] %d %d %s:%s():%d - ") + msg + "\n").c_str(), millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__);
#define WARN_VAR(msg, ...) Serial.printf((String("[WARN] %d %d %s:%s():%d - ") + msg + "\n").c_str(), millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, __VA_ARGS__);
#endif

#ifdef DEBUG_LOGGING_ENABLED
#define DEBUG(msg) Serial.printf((String("[DEBUG] %d %d %s:%s():%d - ") + msg + "\n").c_str(), millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__);
#define DEBUG_VAR(msg, ...) Serial.printf((String("[DEBUG] %d %d %s:%s():%d - ") + msg + "\n").c_str(), millis(), xPortGetCoreID(), __FILE__, __func__, __LINE__, __VA_ARGS__);
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