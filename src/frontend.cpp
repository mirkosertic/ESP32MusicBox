#include "frontend.h"

#include <SPIFFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include <esp_task_wdt.h>

#include "logging.h"
#include "generated_html_templates.h"

String categorizeRSSI(int rssi)
{
  if (rssi >= -50)
  {
    return "Good";
  }
  else if (rssi >= -60)
  {
    return "Okay";
  }
  else if (rssi >= -70)
  {
    return "Poor";
  }
  else
  {
    return "Bad";
  }
}

Frontend::Frontend(FS *fs, App *app, uint16_t wsportnumber, const char *ext, Settings *settings)
{
  this->fs = fs;
  this->app = app;
  this->wsport = wsportnumber;
  this->server = new MongooseHttpServer();
  this->ext = ext;
  this->settings = settings;
}

Frontend::~Frontend()
{
  delete this->server;
}

void Frontend::initialize()
{
  this->server->on("/$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      INFO("webserver() - Rendering status page");
      /*IPAddress remote = request->client()->remoteIP();

      IPAddress apSubnet(192, 168, 4, 0);
      IPAddress apSubnetMask(255, 255, 255, 0);
      if ((remote & apSubnetMask) == (apSubnet & apSubnetMask))
      {
        INFO("Root request thru AP IP - Redirecting to configuration page");
        AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
        response->addHeader("Cache-Control","no-cache, must-revalidate");
        response->addHeader("Location","/settings.html");
        request->send(response);
        return;
      }*/

      unsigned int stackHighWatermark = uxTaskGetStackHighWaterMark(nullptr);
      INFO_VAR("webserver() - Free HEAP is %d, stackHighWatermark is %d", ESP.getFreeHeap(), stackHighWatermark);

      // Chunked response to optimize RAM usage
      size_t content_length = strlen(INDEX_TEMPLATE);

      INFO_VAR("webserver() - Size of response is %d", content_length);
      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->addHeader("Cache-Control", "no-cache, must-revalidate");
      response->setContentType("text/html");
      response->setContentLength(content_length);
      response->print(INDEX_TEMPLATE);

      request->send(response); });

  this->server->on("/status.json$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      INFO("webserver() - Rendering index json");

      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->setContentType("application/json");
      response->setCode(200);
      response->addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonDocument result;

      if (this->app->getTagPresent())
      {
        JsonObject tagscanner = result["tagscanner"].to<JsonObject>();
        tagscanner["tagname"] = this->app->getTagName();
        if (this->app->getIsKnownTag())
        {
          tagscanner["info"] = this->app->getTagInfoText();
        }
        else
        {
          tagscanner["info"] = "Unknown";
        }
      }

      result["appname"] = this->app->getName();
      result["wifistatus"] = categorizeRSSI(WiFi.RSSI());    
      result["stateversion"] = this->app->getStateVersion();
      result["playingstatus"] = this->app->isActive() ? "Playing" : "Stopped";
      result["volume"] = (int)(this->app->getVolume() * 100);
      result["playbackprogress"] = this->app->playProgressInPercent();

      const char *songinfo = this->app->currentTitle();
      if (songinfo)
      {
        result["songinfo"] = songinfo;
      }
      else
      {
        result["songinfo"] = "-";
      }

      serializeJson(result, *response);

      request->send(response); });

  this->server->on("/files.json$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      INFO("webserver() - Rendering index json");

      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->setContentType("application/json");
      response->setCode(200);
      response->addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonDocument result;

      INFO("webserver() - FS scan");
      File root;
      if (request->hasParam("path"))
      {
        String path = request->getParam("path");
        root = fs->open(path);
      }
      else
      {
        root = fs->open("/");
      }

      if (!root)
      {
        result["success"] = false;
        result["info"] = "root not found!";
      }
      else
      {
        String base(root.path());
        result["success"] = true;
        result["basepath"] = base;

        if (base.length() > 1)
        {
          result["base"] = base.substring(1);
          result["parent"] = base.substring(0, base.lastIndexOf("/") + 1);
        }
        else
        {
          result["base"] = base;
        }

        JsonArray files = result["files"].to<JsonArray>();

        int index = 0;
        while (true)
        {
          File entry = root.openNextFile();
          if (!entry)
          {
            break;
          }

          String name(entry.name());
          if (entry.isDirectory())
          {
            JsonObject obj = files.add<JsonObject>();
            obj["dirname"] = name;
            obj["path"] = String(entry.path());
          }
          else
          {
            if (name.endsWith(this->ext))
            {
              JsonObject obj = files.add<JsonObject>();
              obj["filename"] = name;
              obj["size"] = entry.size();
              obj["lastwrite"] = entry.getLastWrite();
              obj["index"] = index++;
            }
          }

          entry.close();
        }

        root.close();
      }
      INFO("webserver() - FS scan done");

      serializeJson(result, *response);

      request->send(response); });

  this->server->on("/networks.html$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      INFO("webserver() - Rendering networks page");

      // Chunked response to optimize RAM usage
      size_t content_length = strlen_P(NETWORKS_TEMPLATE);
      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->addHeader("Cache-Control", "no-cache, must-revalidate");
      response->setContentType("text/html");
      response->setContentLength(content_length);
      response->print(NETWORKS_TEMPLATE);

      request->send(response); });

  this->server->on("/networks.json$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      INFO("webserver() - Rendering networks JSON page");

      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->setContentType("application/json");
      response->setCode(200);
      response->addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonDocument result;

      result["appname"] = this->app->getName();
      result["wifistatus"] = categorizeRSSI(WiFi.RSSI());

      JsonArray networks = result["networks"].to<JsonArray>();

      esp_task_wdt_delete(NULL);
      int n = WiFi.scanNetworks();

      esp_task_wdt_init(30, true);
      esp_task_wdt_add(NULL);

      if (n > 0)
      {
        for (int i = 0; i < n; ++i)
        {
          JsonObject network = networks.add<JsonObject>();
          network["ssid"] = String(WiFi.SSID(i).c_str());
          network["rssi"] = WiFi.RSSI(i);
          network["rssitext"] = categorizeRSSI(WiFi.RSSI(i));
          network["channel"] = WiFi.channel(i);
          switch (WiFi.encryptionType(i))
          {
          case WIFI_AUTH_OPEN:
            network["encryption"] = "open";
            break;
          case WIFI_AUTH_WEP:
            network["encryption"] = "WEP";
            break;
          case WIFI_AUTH_WPA_PSK:
            network["encryption"] = "WPA";
            break;
          case WIFI_AUTH_WPA2_PSK:
            network["encryption"] = "WPA2";
            break;
          case WIFI_AUTH_WPA_WPA2_PSK:
            network["encryption"] = "WPA+WPA2";
            break;
          case WIFI_AUTH_WPA2_ENTERPRISE:
            network["encryption"] = "WPA2-EAP";
            break;
          case WIFI_AUTH_WPA3_PSK:
            network["encryption"] = "WPA3";
            break;
          case WIFI_AUTH_WPA2_WPA3_PSK:
            network["encryption"] = "WPA2+WPA3";
            break;
          case WIFI_AUTH_WAPI_PSK:
            network["encryption"] = "WAPI";
            break;
          default:
            network["encryption"] = "unknown";
          }
          network["bssid"] = String(WiFi.BSSIDstr(i).c_str());
        }
      }

      // Delete the scan result to free memory for code below.
      WiFi.scanDelete();

      serializeJson(result, *response);

      request->send(response); });

  this->server->on("/settings.html$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      INFO("webserver() - Rendering settings page");

      // Chunked response to optimize RAM usage
      size_t content_length = strlen_P(SETTINGS_TEMPLATE);
      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->addHeader("Cache-Control", "no-cache, must-revalidate");
      response->setContentType("text/html");
      response->setContentLength(content_length);
      response->print(SETTINGS_TEMPLATE);

      request->send(response); });

  this->server->on("/settings.json$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      INFO("webserver() - Rendering settings JSON page");                    

      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->setContentType("application/json");
      response->setCode(200);
      response->addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonDocument result;

      result["appname"] = this->app->getName();
      result["wifistatus"] = categorizeRSSI(WiFi.RSSI());    
      result["settingsjson"] = this->settings->getSettingsAsJson();   

      serializeJson(result, *response);

      request->send(response); });

  this->server->on("/updateconfig$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

      String jsonconfig = request->getParam("configdata");
      INFO_VAR("Got new config payload : %s", jsonconfig.c_str());

      this->settings->setSettingsFromJson(jsonconfig);

      MongooseHttpServerResponseStream *response = request->beginResponseStream();
      response->addHeader("Cache-Control", "no-cache, must-revalidate");
      response->addHeader("Location", "/settings.html");

      request->send(response); });

  // Toggle start stop
  this->server->on("/startstop$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

    INFO("webserver() - /startstop received");
    this->app->toggleActiveState();

    JsonDocument result;

    MongooseHttpServerResponseStream *response = request->beginResponseStream();
    response->setCode(200);
    response->setContentType("application/json");
    response->addHeader("Cache-Control","no-cache, must-revalidate");

    serializeJson(result, *response);

    request->send(response); });

  // Next
  this->server->on("/next$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {

    INFO("webserver() - /next received");
    this->app->next();

    JsonDocument result;

    MongooseHttpServerResponseStream *response = request->beginResponseStream();
    response->setCode(200);
    response->setContentType("application/json");
    response->addHeader("Cache-Control","no-cache, must-revalidate");

    serializeJson(result, *response);

    request->send(response); });

  // Previous
  this->server->on("/previous$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {
    
    INFO("webserver() - /previous received");
    this->app->previous();

    JsonDocument result;

    MongooseHttpServerResponseStream *response = request->beginResponseStream();
    response->setCode(200);
    response->setContentType("application/json");
    response->addHeader("Cache-Control","no-cache, must-revalidate");

    serializeJson(result, *response);

    request->send(response); });

  // play
  this->server->on("/play$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {
    
    INFO("webserver() - /play received");

    String path = request->getParam("path");
    int index = request->getParam("index").toInt();

    this->app->play(path, index);

    JsonDocument result;    

    MongooseHttpServerResponseStream *response = request->beginResponseStream();
    response->setCode(200);
    response->setContentType("application/json");
    response->addHeader("Cache-Control","no-cache, must-revalidate");

    serializeJson(result, *response);

    request->send(response); });

  // volume
  this->server->on("/volume$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {
    
    INFO("webserver() - /volume received");

    int volume = request->getParam("volume").toInt();

    this->app->setVolume(volume / 100.0f);

    JsonDocument result;

    MongooseHttpServerResponseStream *response = request->beginResponseStream();
    response->setCode(200);
    response->setContentType("application/json");
    response->addHeader("Cache-Control","no-cache, must-revalidate");

    serializeJson(result, *response);

    request->send(response); });

  // assign
  this->server->on("/assign$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {
    
    INFO("webserver() - /assign received");

    CommandData command;
    memset(&command, 0, 44);
    command.version = COMMAND_VERSION;
    command.command = COMMAND_PLAY_DIRECTORY;
    command.index = 0;
    command.volume = (uint8_t) (this->app->getVolume() * 100);

    String path = request->getParam("path");

    INFO_VAR("webserver() - Path is %s", path.c_str());

    memcpy(&command.path[0], path.c_str(), path.length());

    this->app->writeCommandToTag(command);

    JsonDocument result;

    MongooseHttpServerResponseStream *response = request->beginResponseStream();
    response->setCode(200);
    response->setContentType("application/json");
    response->addHeader("Cache-Control","no-cache, must-revalidate");

    serializeJson(result, *response); });

  // delete
  this->server->on("/delete$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {
    INFO("webserver() - /delete received");

    this->app->clearTag();

    JsonDocument result;

    MongooseHttpServerResponseStream *response = request->beginResponseStream();
    response->setCode(200);
    response->setContentType("application/json");
    response->addHeader("Cache-Control","no-cache, must-revalidate");

    serializeJson(result, *response);

    request->send(response); });

  this->server->on("/description.xml$", HTTP_GET, [this](MongooseHttpServerRequest *request)
                   {
                     INFO("webserver() - /description.xml received");

                    MongooseHttpServerResponseStream *response = request->beginResponseStream();
                    response->setCode(200);
                    response->setContentType("application/xml");
                    response->addHeader("Cache-Control","no-cache, must-revalidate");      
                    response->print(app->getSSDPDescription());

                    request->send(response); });

  this->server->onNotFound([this](MongooseHttpServerRequest *request)
                           {
              INFO("webserver() - Not Found");

              MongooseHttpServerResponseStream *response = request->beginResponseStream();
              response->setCode(404);
              response->setContentType("text/plain");
              response->addHeader("Cache-Control","no-cache, must-revalidate");      

              request->send(response); });
}

void Frontend::begin()
{
  Mongoose.begin();

  this->server->begin(this->wsport);

  this->initialize();
}

void Frontend::loop()
{
  Mongoose.poll(5);
}
