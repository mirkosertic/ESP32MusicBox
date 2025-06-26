#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <PsychicHttp.h>
#include <FS.h>
#include <WiFiUdp.h>

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
  WiFiUDP *udp;

  void initialize();
  void announceMDNS();
  void announceSSDP();
  void ssdpNotify();

  String getSSDPDescription();

public:
  Webserver(FS *fs, App *app, uint16_t wsportnumber, const char *ext, Settings *settings);
  ~Webserver();

  String getConfigurationURL();

  void begin();

  void loop();
};
#endif
