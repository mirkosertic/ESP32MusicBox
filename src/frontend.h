#ifndef FRONTEND_H
#define FRONTEND_H

#include <Arduino.h>

// For some random reason this is required to make ESPAsyncWebServer compile without errors
//#include <WebServer.h>

#ifndef _ESPAsyncWebServer_H_
#define _ESPAsyncWebServer_H_
#endif

#include <WebServer.h>
#include <MongooseCore.h>
#include <MongooseHttpServer.h>
#include <FS.h>

#include "app.h"
#include "settings.h"

class Frontend
{
private:
  uint16_t wsport;
  FS *fs;
  App *app;
  MongooseHttpServer *server;
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