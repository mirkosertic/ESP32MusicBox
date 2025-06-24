#ifndef FRONTEND_H
#define FRONTEND_H

#include <PsychicHttp.h>
#include <FS.h>

#include "app.h"
#include "settings.h"

class Frontend
{
private:
  uint16_t wsport;
  FS *fs;
  App *app;
  PsychicHttpServer *server;
  const char *ext;
  Settings *settings;

  void initialize();

public:
  Frontend(FS *fs, App *app, uint16_t wsportnumber, const char *ext, Settings *settings);
  ~Frontend();

  void begin();

  void loop();
};
#endif
