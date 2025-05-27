#ifndef FRONTEND_H
#define FRONTEND_H

// For some random reason this is required to make ESPAsyncWebServer compile without errors
#include <WebServer.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>

#include "app.h"
#include "settings.h"

class Frontend
{
private:
  FS *fs;
  App *app;
  AsyncWebServer *server;
  const char *ext;
  Settings *settings;

  void initialize();

public:
  Frontend(FS *fs, App *app, uint16_t wsportnumber, const char *ext, Settings *settings);

  ~Frontend();

  void begin();
};

#endif