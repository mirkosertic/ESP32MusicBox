#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <PsychicHttp.h>
#include <FS.h>

#include "app.h"
#include "settings.h"

class Webserver
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
  Webserver(FS *fs, App *app, uint16_t wsportnumber, const char *ext, Settings *settings);
  ~Webserver();

  void begin();

  void loop();
};
#endif
