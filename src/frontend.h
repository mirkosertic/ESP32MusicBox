#ifndef FRONTEND_H
#define FRONTEND_H

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
public:
  Frontend(FS *fs, App *app, uint16_t wsportnumber, const char *ext, Settings *settings);

  ~Frontend();

  void initialize();

  void begin();
};

#endif