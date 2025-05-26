#include "frontend.h"

#include <ArduinoJson.h>
#include <Ministache.h>
#include <WiFi.h>

#include "esp_task_wdt.h"

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
  this->server = new AsyncWebServer(wsportnumber);
  this->ext = ext;
  this->settings = settings;
}

Frontend::~Frontend()
{
  this->server->end();
  delete this->server;
}

void Frontend::initialize()
{
  this->server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {

    IPAddress remote = request->client()->remoteIP();

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
    }

    JsonDocument result;
    result["appname"] = this->app->getName();
    result["wifistatus"] = categorizeRSSI(WiFi.RSSI());

    if (this->app->getTagPresent()) {
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

    result["stateversion"] = this->app->getStateVersion();
    result["playingstatus"] = this->app->isActive() ? "Playing" : "Stopped";
    result["volume"] = (int) (this->app->getVolume() * 100);
    result["playbackprogress"] = this->app->playProgressInPercent();

    File root;
    if (request->hasParam("path")) {
      String path = request->getParam("path")->value();
      root = fs->open(path);
    } else {
      root = fs->open("/");
    }

    if (!root) {
      result["success"] = false;
      result["info"] = "root not found!";

    } else {

      const char *songinfo = this->app->currentTitle();
      if (songinfo) {
        result["songinfo"] = songinfo;
      } else {
        result["songinfo"] = "-";
      }
      
      String base(root.path());
      result["success"] = true;
      result["basepath"] = base;

      if (base.length() > 1) {
        result["base"] = base.substring(1);
        result["parent"] = base.substring(0, base.lastIndexOf("/") + 1);
      } else {
        result["base"] = base;
      }

      JsonArray files = result["files"].to<JsonArray>();

      int index = 0;
      while(true) {
        File entry = root.openNextFile();
        if (! entry) {
          break;
        }

        String name(entry.name());
        if (entry.isDirectory()) {
          JsonObject obj = files.add<JsonObject>();
          obj["dirname"] = name;
          obj["path"] = String(entry.path());
        } else {
          if (name.endsWith(this->ext)) {
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

    const String output = ministache::render(INDEX_TEMPLATE, result);

    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", output);
    response->addHeader("Cache-Control","no-cache, must-revalidate");
    request->send(response); });

  this->server->on("/", HTTP_HEAD, [this](AsyncWebServerRequest *request)
                   {

    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "");
    response->addHeader("Cache-Control","no-cache, must-revalidate");
    response->addHeader("x-stateversion",String(this->app->getStateVersion()));
    request->send(response); });

  this->server->on("/networks.html", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {

    JsonDocument result;

    result["appname"] = this->app->getName();
    result["wifistatus"] = categorizeRSSI(WiFi.RSSI());    

    JsonArray networks = result["networks"].to<JsonArray>();

    esp_task_wdt_delete(NULL);
    int n = WiFi.scanNetworks();

    /*esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,    // Bitmask of all cores
        .trigger_panic = false,
    };

    esp_task_wdt_init(&wdt_config);*/ // 30 Sekunden Timeout
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);

    if (n > 0) {
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

    const String output = ministache::render(NETWORKS_TEMPLATE, result);
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", output);
    response->addHeader("Cache-Control","no-cache, must-revalidate");
    request->send(response); });

  this->server->on("/settings.html", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {

    JsonDocument result;

    result["appname"] = this->app->getName();
    result["wifistatus"] = categorizeRSSI(WiFi.RSSI());    
    result["settingsjson"] = this->settings->getSettingsAsJson();    

    const String output = ministache::render(SETTINGS_TEMPLATE, result);
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", output);
    response->addHeader("Cache-Control","no-cache, must-revalidate");
    request->send(response); });

  this->server->on("/updateconfig", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
                     String jsonconfig = request->getParam("configdata")->value();
                     INFO_VAR("Got new config payload : %s", jsonconfig.c_str());

                     this->settings->setSettingsFromJson(jsonconfig);

                     AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
                     response->addHeader("Cache-Control", "no-cache, must-revalidate");
                     response->addHeader("Location", "/settings.html");
                     request->send(response); });

  // Toggle start stop
  this->server->on("/startstop", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
    INFO("/startstop received");
    this->app->toggleActiveState();

    if (request->hasParam("path")) {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");      
      response->addHeader("Location","/?path=" + request->getParam("path")->value());
      request->send(response);       
    } else {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/");
      request->send(response);       
    } });

  // Next
  this->server->on("/next", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
    INFO("/next received");
    this->app->next();

    if (request->hasParam("path")) {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/?path=" + request->getParam("path")->value());
      request->send(response);       
    } else {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/");
      request->send(response);       
    } });

  // Previous
  this->server->on("/previous", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
    
    INFO("/previous received");
    this->app->previous();

    if (request->hasParam("path")) {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/?path=" + request->getParam("path")->value());
      request->send(response);       
    } else {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/");
      request->send(response);       
    } });

  // play
  this->server->on("/play", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
    
    INFO("/play received");

    String path = request->getParam("path")->value();
    int index = request->getParam("index")->value().toInt();

    this->app->play(path, index);

    AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
    response->addHeader("Cache-Control","no-cache, must-revalidate");            
    response->addHeader("Location","/?path=" + path);
    request->send(response); });

  // volume
  this->server->on("/volume", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
    
    INFO("webserver() - /volume received");

    String path = request->getParam("path")->value();
    int volume = request->getParam("volume")->value().toInt();

    this->app->setVolume(volume / 100.0f);

    AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
    response->addHeader("Cache-Control","no-cache, must-revalidate");            
    response->addHeader("Location","/?path=" + path);
    request->send(response); });

  // assign
  this->server->on("/assign", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
    
    INFO("/assign received");

    CommandData command;
    memset(&command, 0, 44);
    command.version = COMMAND_VERSION;
    command.command = COMMAND_PLAY_DIRECTORY;
    command.index = 0;
    command.volume = (uint8_t) (this->app->getVolume() * 100);

    const char *path = request->getParam("path")->value().c_str();
    memcpy(&command.path[0], path, strlen(path));

    this->app->writeCommandToTag(command);

    if (request->hasParam("path")) {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/?path=" + request->getParam("path")->value());
      request->send(response);       
    } else {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/");
      request->send(response);       
    } });

  // delete
  this->server->on("/delete", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
    INFO("/delete received");

    this->app->clearTag();

    if (request->hasParam("path")) {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/?path=" + request->getParam("path")->value());
      request->send(response);       
    } else {
      AsyncWebServerResponse *response = request->beginResponse(302, "text/html");
      response->addHeader("Cache-Control","no-cache, must-revalidate");            
      response->addHeader("Location","/");
      request->send(response);       
    } });

  this->server->on("/description.xml", HTTP_GET,
                   [&](AsyncWebServerRequest *request)
                   {
                     INFO("/description.xml received");

                     request->send(200, "text/xml", app->getSSDPDescription());
                   });

  this->server->onNotFound([&](AsyncWebServerRequest *request)
                           {
              INFO_VAR("Not Found : %s Method %s", request->url().c_str(), request->methodToString());
              request->send(404, "text/plain", "Not found"); });
}

void Frontend::begin()
{
  this->initialize();
  this->server->begin();
}
