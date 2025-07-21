#include <Arduino.h>

#include "webserver.h"

#include "generated_html_templates.h"
#include "logging.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

bool createDirectoryRecursive(FS *fs, const String &path) {
	if (fs->exists(path)) {
		return true;
	}

	int lastSlash = path.lastIndexOf('/');

	if (lastSlash <= 0) {
		return fs->mkdir(path);
	}

	String parentPath = path.substring(0, lastSlash);

	if (!createDirectoryRecursive(fs, parentPath)) {
		return false;
	}

	return fs->mkdir(path);
}

class CustomPsychicStreamResponse : public PsychicResponse, public Print {
private:
	ChunkPrinter *_printer;
	uint8_t *_buffer;

public:
	CustomPsychicStreamResponse(PsychicRequest *request, const String &contentType)
		: PsychicResponse(request)
		, _buffer(NULL) {

		setContentType(contentType.c_str());
		addHeader("Content-Disposition", "inline");
	}

	~CustomPsychicStreamResponse() {
		endSend();
	}

	esp_err_t beginSend() {
		if (_buffer) {
			return ESP_OK;
		}

		// Buffer to hold ChunkPrinter and stream buffer. Using placement new will keep us at a single allocation.
		_buffer = (uint8_t *) malloc(STREAM_CHUNK_SIZE + sizeof(ChunkPrinter));

		if (!_buffer) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(_request->request(), HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to allocate memory.");
			return ESP_FAIL;
		}

		// esp-idf makes you set the whole status.
		sprintf(_status, "%u %s", _code, http_status_reason(_code));
		httpd_resp_set_status(_request->request(), _status);

		_printer = new (_buffer) ChunkPrinter(this, _buffer + sizeof(ChunkPrinter), STREAM_CHUNK_SIZE);

		sendHeaders();
		return ESP_OK;
	}

	esp_err_t endSend() {
		esp_err_t err = ESP_OK;

		if (!_buffer) {
			err = ESP_FAIL;
		} else {
			_printer->~ChunkPrinter(); // flushed on destruct
			err = finishChunking();
			free(_buffer);
			_buffer = NULL;
		}
		return err;
	}

	void flush() override {
		if (_buffer) {
			_printer->flush();
		}
	}

	size_t write(uint8_t data) override {
		return _buffer ? _printer->write(data) : 0;
	}

	size_t write(const uint8_t *buffer, size_t size) override {
		return _buffer ? _printer->write(buffer, size) : 0;
	}

	size_t copyFrom(Stream &stream) {
		if (_buffer) {
			return _printer->copyFrom(stream);
		}

		return 0;
	}

	using Print::write;
};

String categorizeRSSI(int rssi) {
	if (rssi >= -50) {
		return "Good";
	} else if (rssi >= -60) {
		return "Okay";
	} else if (rssi >= -70) {
		return "Poor";
	} else {
		return "Bad";
	}
}

String urlDecode(String input) {
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = input.length();
	unsigned int i = 0;

	while (i < len) {
		char decodedChar;
		char actualChar = input.charAt(i);

		if (actualChar == '%') {
			// Check if we have enough characters for hex decoding
			if (i + 2 < len) {
				temp[2] = input.charAt(i + 1);
				temp[3] = input.charAt(i + 2);
				decodedChar = strtol(temp, NULL, 16);
				i += 3;
			} else {
				// Invalid encoding, just add the % character
				decodedChar = actualChar;
				i++;
			}
		} else if (actualChar == '+') {
			// Convert + to space
			decodedChar = ' ';
			i++;
		} else {
			// Regular character
			decodedChar = actualChar;
			i++;
		}

		decoded += decodedChar;
	}

	return decoded;
}

String extractWebDAVPath(PsychicRequest *request) {
	String uri = request->uri().c_str();
	if (uri.startsWith("/webdav/")) {
		String p = uri.substring(7);
		int len = p.length();
		if (len > 1 && p.endsWith("/")) {
			return urlDecode(p.substring(0, len - 1));
		}
		return urlDecode(p);
	}
	return urlDecode(uri);
}

String urlencode(String str) {
	String encodedString = "";
	char c;
	char code0;
	char code1;
	for (int i = 0; i < str.length(); i++) {
		c = str.charAt(i);
		if (c == ' ') {
			encodedString += "%20";
		} else if (isalnum(c)) {
			encodedString += c;
		} else {
			code1 = (c & 0xf) + '0';
			if ((c & 0xf) > 9) {
				code1 = (c & 0xf) - 10 + 'A';
			}
			c = (c >> 4) & 0xf;
			code0 = c + '0';
			if (c > 9) {
				code0 = c - 10 + 'A';
			}
			encodedString += '%';
			encodedString += code0;
			encodedString += code1;
			// encodedString+=code2;
		}
	}
	return encodedString;
}

Webserver::Webserver(FS *fs, App *app, uint16_t wsportnumber, const char *ext, Settings *settings) {
	this->fs = fs;
	this->app = app;
	this->wsport = wsportnumber;
	this->server = new PsychicHttpServer();
	this->ext = ext;
	this->settings = settings;
}

Webserver::~Webserver() {
	delete this->server;
}

void Webserver::initialize() {
	this->server->on("/", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering / page");
      IPAddress remote = request->client()->remoteIP();

      IPAddress apSubnet(192, 168, 4, 0);
      IPAddress apSubnetMask(255, 255, 255, 0);
      if ((remote & apSubnetMask) == (apSubnet & apSubnetMask))
      {
        INFO("Root request thru AP IP - Redirecting to configuration page");
        CustomPsychicStreamResponse response(request, "text/html");
        response.setCode(302);
        response.addHeader("Cache-Control","no-cache, must-revalidate");
        response.addHeader("Location","/settings.html");
        response.setContentLength(0),

        response.beginSend();
        return response.endSend();
      }

      unsigned int stackHighWatermark = uxTaskGetStackHighWaterMark(nullptr);
      INFO("webserver() - Free HEAP is %d, stackHighWatermark is %d", ESP.getFreeHeap(), stackHighWatermark);

      // Chunked response to optimize RAM usage
      size_t content_length = strlen(INDEX_TEMPLATE);

      INFO("webserver() - Size of response is %d", content_length);
      CustomPsychicStreamResponse response(request, "text/html");

      response.addHeader("Cache-Control", "no-cache, must-revalidate");
      response.setContentType("text/html");
      response.setContentLength(content_length);

      response.beginSend();
      response.print(INDEX_TEMPLATE);
      return response.endSend(); });

	this->server->on("/status.json", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering /status.json");

      PsychicJsonResponse response = PsychicJsonResponse(request);
      response.setContentType("application/json");
      response.setCode(200);
      response.addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonObject result = response.getRoot();

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

      return response.send(); });

	this->server->on("/files.json", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering /files.json");

      PsychicJsonResponse response = PsychicJsonResponse(request);
      response.setContentType("application/json");
      response.setCode(200);
      response.addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonObject result = response.getRoot();

      INFO("webserver() - FS scan");
      File root;
      if (request->hasParam("path"))
      {
        String path = request->getParam("path")->value();
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

      return response.send(); });

	this->server->on("/networks.html", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering /networks.html");

      // Chunked response to optimize RAM usage
      size_t content_length = strlen_P(NETWORKS_TEMPLATE);

      CustomPsychicStreamResponse response(request, "text/html");
      response.addHeader("Cache-Control", "no-cache, must-revalidate");
      response.setContentType("text/html");
      response.setContentLength(content_length);

      response.beginSend();
      response.print(NETWORKS_TEMPLATE);

      return response.endSend(); });

	this->server->on("/networks.json", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering /networks.json");

      PsychicJsonResponse response = PsychicJsonResponse(request);
      response.setContentType("application/json");
      response.setCode(200);
      response.addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonObject result = response.getRoot();

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

      return response.send(); });

	this->server->on("/settings.html", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering /settings.html");

      // Chunked response to optimize RAM usage
      size_t content_length = strlen_P(SETTINGS_TEMPLATE);
      
      CustomPsychicStreamResponse response(request, "text/html");
      response.setContentType("text/html");
      response.addHeader("Cache-Control", "no-cache, must-revalidate");
      response.setContentLength(content_length);

      response.beginSend();
      response.print(SETTINGS_TEMPLATE);

      return response.endSend(); });

	this->server->on("/settings.json", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering /settings.json");

      PsychicJsonResponse response = PsychicJsonResponse(request);
      response.setContentType("application/json");
      response.setCode(200);
      response.addHeader("Cache-Control", "no-cache, must-revalidate");

      JsonObject result = response.getRoot();

      result["appname"] = this->app->getName();
      result["wifistatus"] = categorizeRSSI(WiFi.RSSI());    
      result["settingsjson"] = this->settings->getSettingsAsJson();   

      return response.send(); });

	this->server->on("/updateconfig", HTTP_POST, [this](PsychicRequest *request) {

      INFO("webserver() - /updateconfig received");

      String jsonconfig = request->getParam("configdata")->value();
      INFO("Got new config payload : %s", jsonconfig.c_str());

      this->settings->setSettingsFromJson(jsonconfig);

      PsychicResponse response(request);
      response.setCode(301);
      response.addHeader("Cache-Control", "no-cache, must-revalidate");
      response.addHeader("Location", "/settings.html");

      return response.send(); });

	this->server->on("/webradio.html", HTTP_GET, [this](PsychicRequest *request) {

      INFO("webserver() - Rendering /webradio.html");

      // Chunked response to optimize RAM usage
      size_t content_length = strlen_P(WEBRADIO_TEMPLATE);
      
      CustomPsychicStreamResponse response(request, "text/html");
      response.setContentType("text/html");
      response.addHeader("Cache-Control", "no-cache, must-revalidate");
      response.setContentLength(content_length);

      response.beginSend();
      response.print(WEBRADIO_TEMPLATE);

      return response.endSend(); });

	// Shutdown
	this->server->on("/shutdown", HTTP_GET, [this](PsychicRequest *request) {

    INFO("webserver() - /shutdown received");
    this->app->shutdown();

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// Toggle start stop
	this->server->on("/startstop", HTTP_GET, [this](PsychicRequest *request) {

    INFO("webserver() - /startstop received");
    this->app->toggleActiveState();

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// Next
	this->server->on("/next", HTTP_GET, [this](PsychicRequest *request) {

    INFO("webserver() - /next received");
    this->app->next();

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// Previous
	this->server->on("/previous", HTTP_GET, [this](PsychicRequest *request) {
    
    INFO("webserver() - /previous received");
    this->app->previous();

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// play
	this->server->on("/play", HTTP_GET, [this](PsychicRequest *request) {
    
    INFO("webserver() - /play received");

    String path = request->getParam("path")->value();
    int index = request->getParam("index")->value().toInt();

    this->app->play(path, index);

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// playwebradio
	this->server->on("/playwebradio", HTTP_GET, [this](PsychicRequest *request) {
    
    INFO("webserver() - /playwebradio received");

    String url = request->getParam("url")->value();

    this->app->playURL(url);

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// volume
	this->server->on("/volume", HTTP_GET, [this](PsychicRequest *request) {
    
    INFO("webserver() - /volume received");

    int volume = request->getParam("volume")->value().toInt();

    this->app->setVolume(volume / 100.0f);

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// assign
	this->server->on("/assign", HTTP_GET, [this](PsychicRequest *request) {
    
    INFO("webserver() - /assign received");

    CommandData command;
    memset(&command, 0, 44);
    command.version = COMMAND_VERSION;
    command.command = COMMAND_PLAY_DIRECTORY;
    command.index = 0;
    command.volume = (uint8_t) (this->app->getVolume() * 100);

    String path = request->getParam("path")->value();

    INFO("webserver() - Path is %s", path.c_str());

    memcpy(&command.path[0], path.c_str(), path.length());

    this->app->writeCommandToTag(command);

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	// delete
	this->server->on("/delete", HTTP_GET, [this](PsychicRequest *request) {
    INFO("webserver() - /delete received");

    this->app->clearTag();

    PsychicJsonResponse response = PsychicJsonResponse(request);
    response.setContentType("application/json");
    response.setCode(200);
    response.addHeader("Cache-Control", "no-cache, must-revalidate");

    // JsonObject result = response.getRoot();

    return response.send(); });

	this->server->on("/description.xml", HTTP_GET, [this](PsychicRequest *request) {
                    INFO("webserver() - /description.xml received");

                    String data = this->getSSDPDescription();

                    CustomPsychicStreamResponse response(request, "application/xml");
                    response.setContentType("application/xml");
                    response.addHeader("Cache-Control", "no-cache, must-revalidate");
                    response.setContentLength(data.length());

                    response.beginSend();
                    response.print(data);
                    return response.endSend(); });

	// WebDav Minimal Level 1 compliance
	this->server->on("/webdav", HTTP_OPTIONS, [this](PsychicRequest *request) {
                    INFO("webserver() - /webdav %s %s received", request->uri().c_str(), request->methodStr().c_str());

                    PsychicResponse response(request);
                    response.setCode(204);
                    response.addHeader("Cache-Control","no-cache, must-revalidate");      
                    response.addHeader("Allow", "OPTIONS, GET, PUT, PROPFIND, MKCOL"); // No DELETE on Root!
                    response.addHeader("DAV", "1");
                    response.addHeader("MS-Author-Via", "DAV");
                    response.setContentLength(0);

                    return response.send(); });

	this->server->on("/webdav/*", HTTP_OPTIONS, [this](PsychicRequest *request) {
                    INFO("webserver() - /webdav %s %s received", request->uri().c_str(), request->methodStr().c_str());

                    String path = extractWebDAVPath(request);
                    File root = fs->open(path);

                    PsychicResponse response(request);

                    if (!root) {
                      WARN("webserver() - Path %s not found", path.c_str());
                      // Not found
                      response.setCode(404);
                      response.addHeader("Cache-Control","no-cache, must-revalidate");      
                      response.addHeader("DAV", "1");
                      response.addHeader("MS-Author-Via", "DAV");
                      response.setContentLength(0);
                    } else {
                      if (root.isDirectory()) {
                        INFO("webserver() - Path %s is directory / collection", path.c_str());
                        response.setCode(204);
                        response.addHeader("Cache-Control","no-cache, must-revalidate");      
                        response.addHeader("Allow", "OPTIONS, GET, PUT, DELETE, PROPFIND, MKCOL");
                        response.addHeader("DAV", "1");
                        response.addHeader("MS-Author-Via", "DAV");
                        response.setContentLength(0);
                      } else {
                        INFO("webserver() - Path %s is file", path.c_str());                        
                        response.setCode(204);
                        response.addHeader("Cache-Control","no-cache, must-revalidate");      
                        response.addHeader("Allow", "OPTIONS, GET, PUT, DELETE, PROPFIND");
                        response.addHeader("DAV", "1");
                        response.addHeader("MS-Author-Via", "DAV");
                        response.setContentLength(0);
                      }
                    }

                    root.close();

                    return response.send(); });

	this->server->on("/webdav/*", HTTP_PROPFIND, [this](PsychicRequest *request) {
                    INFO("webserver() - /webdav %s %s received", request->uri().c_str(), request->methodStr().c_str());

                    String path = extractWebDAVPath(request);

                    int depth = 1;
                    if (request->hasParam("Depth")) {
                      depth = request->getParam("Depth")->value().toInt();
                    }

                    INFO("webserver() - /webdav %s PROPFIND depth %d path %s received", path.c_str(), depth, path.c_str());

                    File root = fs->open(path);
                    if (!root) {
                      WARN("webserver() - Path not found!");

                      PsychicResponse response(request);
                      response.addHeader("Cache-Control","no-cache, must-revalidate");                          
                      response.setCode(404);
                      response.setContentLength(0);

                      return response.send();
                    }
                    INFO("webserver() - Generating listing");

                    CustomPsychicStreamResponse response(request, "application/xml");
                    response.setCode(207);
                    response.setContentType("application/xml; charset=utf-8");
                    response.addHeader("Cache-Control","no-cache, must-revalidate");      

                    response.beginSend();

                    response.println("<?xml version=\"1.0\" encoding=\"utf-8\" ?>");
                    response.println("<D:multistatus xmlns:D=\"DAV:\">");

                    while (true)
                    {
                      File entry = root.openNextFile();
                      if (!entry)
                      {
                        break;
                      }

                      String name(entry.name());

                      INFO("webserver() - Found file path %s name %s", path.c_str(), name.c_str());

                      response.println("  <D:response>");
                      if (path.startsWith("/")) {
                        response.print("    <D:href>/webdav");
                        response.print(urlencode(path));
                      } else {
                        response.print("    <D:href>/webdav/");
                        response.print(urlencode(path));
                      }

                      if (path.endsWith("/"))
                      {
                        response.print(urlencode(name));
                        if (entry.isDirectory()) {
                          response.print("/");
                        }
                        response.println("</D:href>");                        
                      }
                      else
                      {
                        response.print("/");
                        response.print(urlencode(name));
                        if (entry.isDirectory()) {
                          response.print("/");
                        }
                        response.println("</D:href>");                        
                      }


                      if (entry.isDirectory())
                      {

                        response.println("    <D:propstat>");
                        response.println("      <D:prop>");
                        response.println("        <D:resourcetype><D:collection/></D:resourcetype>");
                        response.println("      </D:prop>");
                        response.println("      <D:status>HTTP/1.1 200 OK</D:status>");
                        response.println("    </D:propstat>");

                      }
                      else
                      {
                        response.println("    <D:propstat>");
                        response.println("      <D:prop>");
                        response.print("        <D:getcontentlength>");
                        response.print(entry.size());
                        response.println("</D:getcontentlength>");
                        response.println("        <D:resourcetype/>");
                        //response->println("        <D:getcontenttype>application/octet-stream</D:getcontenttype>");
                        //response->println("        <D:getlastmodified>Fri, 30 Sep 2016 23:28:31 +0000</D:getlastmodified>");
                        response.println("      </D:prop>");
                        response.println("      <D:status>HTTP/1.1 200 OK</D:status>");
                        response.println("    </D:propstat>");

                      }

                      response.println("  </D:response>");                      

                      entry.close();
                    }
                    response.println("</D:multistatus>");

                    root.close();                        

                    return response.endSend(); });

	this->server->on("/webdav/*", HTTP_MKCOL, [this](PsychicRequest *request) {
                    INFO("webserver() - /webdav %s %s received", request->uri().c_str(), request->methodStr().c_str());

                    String path = extractWebDAVPath(request);

                    CustomPsychicStreamResponse response(request, "application/xml");                    
                    if (createDirectoryRecursive(fs, path)) {
                      INFO("webserver() - Path %s created", path.c_str());

                      response.setCode(201);
                      response.addHeader("Cache-Control","no-cache, must-revalidate");      
                      response.addHeader("DAV", "1");
                      response.addHeader("MS-Author-Via", "DAV");
                      response.setContentLength(0);

                    } else {

                      WARN("webserver() - Path %s NOT created", path.c_str());

                      response.setCode(409);
                      response.addHeader("Cache-Control","no-cache, must-revalidate");      
                      response.addHeader("DAV", "1");
                      response.addHeader("MS-Author-Via", "DAV");
                      response.setContentLength(0);
                    }

                    return response.send(); });

	PsychicUploadHandler *puthandler = new PsychicUploadHandler();
	puthandler->onUpload([this](PsychicRequest *request, const String &filename, uint64_t position, uint8_t *data, size_t length, bool final) {
      INFO("webserver() - got data chunk with position %llu and length %u", position, length);

      String path = extractWebDAVPath(request);

      File content;
      if (position == 0) {
        INFO("webserver() - creating file %s", path.c_str());
        content = fs->open(path, FILE_WRITE, true);
      } else {
        INFO("webserver() - opening file %s for append", path.c_str());        
        content = fs->open(path, FILE_APPEND);
      }

      if (!content) {
        WARN("webserver() - failed to access file");
        return ESP_FAIL;
      } 

      if (!content.write(data, length)) {
        WARN("webserver() - Failed wo write data");
        return ESP_FAIL;
      }

      close(content);

      return ESP_OK; });
	puthandler->onRequest([this](PsychicRequest *request) {
      CustomPsychicStreamResponse response(request, "application/xml");                    

      String path = extractWebDAVPath(request);      

      INFO("webserver() - content %s created", path.c_str());

      response.setCode(201);
      response.addHeader("Cache-Control","no-cache, must-revalidate");      
      response.addHeader("DAV", "1");
      response.addHeader("MS-Author-Via", "DAV");
      response.setContentLength(0);

      return response.send(); });
	this->server->on("/webdav/*", HTTP_PUT, puthandler);

	this->server->on("/webdav/*", HTTP_GET, [this](PsychicRequest *request) {
                    INFO("webserver() - /webdav %s %s received", request->uri().c_str(), request->methodStr().c_str());

                    String path = extractWebDAVPath(request);

                    File content = fs->open(path, "r");
                    
                    // TODO: Fix WDT
                    CustomPsychicStreamResponse response(request, "application/octet-stream");
                    INFO("webserver() - content %s requested for download", path.c_str());
                    response.setCode(200);
                                        response.addHeader("Cache-Control","no-cache, must-revalidate");      
                    response.addHeader("DAV", "1");
                    response.addHeader("MS-Author-Via", "DAV");

                    response.beginSend();

                    INFO("webserver() - start sending data to client");
                    char buffer[512];
                    size_t read = content.readBytes(&buffer[0], sizeof(buffer));
                    while (read > 0) {
                      response.write((uint8_t *)&buffer[0], read);
                      read = content.readBytes(&buffer[0], sizeof(buffer));
                    }
                    INFO("webserver() - transfer finished");

                    content.close();                    

                    return response.endSend(); });

	this->server->on("/webdav/*", HTTP_DELETE, [this](PsychicRequest *request) {
                    INFO("webserver() - /webdav %s %s received", request->uri().c_str(), request->methodStr().c_str());

                    String path = extractWebDAVPath(request);

                    File root = fs->open(path);

                    PsychicResponse response(request);

                    if (!root) {
                      WARN("webserver() - Path %s not found", path.c_str());
                      // Not found
                      response.setCode(404);
                      response.addHeader("Cache-Control","no-cache, must-revalidate");      
                      response.addHeader("DAV", "1");
                      response.addHeader("MS-Author-Via", "DAV");
                      response.setContentLength(0);
                    } else {
                      boolean isdir = root.isDirectory();
                      root.close();

                      if (isdir) {
                        if (fs->rmdir(path.c_str())) {
                          INFO("webserver() - directory %s removed", path.c_str());
                          response.setCode(201);
                          response.addHeader("Cache-Control","no-cache, must-revalidate");      
                          response.addHeader("DAV", "1");
                          response.addHeader("MS-Author-Via", "DAV");
                          response.setContentLength(0);
                        } else {
                          WARN("webserver() - directory %s NOT removed", path.c_str());                          
                          response.setCode(404);
                          response.addHeader("Cache-Control","no-cache, must-revalidate");      
                          response.addHeader("DAV", "1");
                          response.addHeader("MS-Author-Via", "DAV");
                          response.setContentLength(0);
                        }
                      } else {
                        if (fs->remove(path.c_str())) {
                          INFO("webserver() - file %s removed", path.c_str());
                          response.setCode(201);
                          response.addHeader("Cache-Control","no-cache, must-revalidate");      
                          response.addHeader("DAV", "1");
                          response.addHeader("MS-Author-Via", "DAV");
                          response.setContentLength(0);
                        } else {
                          WARN("webserver() - file %s NOT removed", path.c_str());                          
                          response.setCode(404);
                          response.addHeader("Cache-Control","no-cache, must-revalidate");      
                          response.addHeader("DAV", "1");
                          response.addHeader("MS-Author-Via", "DAV");
                          response.setContentLength(0);
                        }
                      }
                    }

                    return response.send(); });

	// Default Handler
	this->server->onNotFound([this](PsychicRequest *request) {
              WARN("webserver() - Not Found %s %s", request->methodStr().c_str(), request->uri().c_str());

              PsychicResponse response(request);
              response.setCode(404);
              response.setContentType("text/plain");
              response.addHeader("Cache-Control","no-cache, must-revalidate");      
              response.setContentLength(0);              

              return response.send(); });
}

String Webserver::getConfigurationURL() {
	return "http://" + this->app->computeTechnicalName() + ".local:" + this->wsport + "/";
}

void Webserver::announceMDNS() {
	String technicalName = this->app->computeTechnicalName();
	if (MDNS.begin(technicalName)) {
		INFO("Registered as mDNS-Name %s", technicalName.c_str());
	} else {
		WARN("Registered as mDNS-Name %s failed", technicalName.c_str());
	}
}

void Webserver::ssdpNotify() {
	INFO("Sent SSDP NOTIFY messages");

	const IPAddress SSDP_MULTICAST_ADDR(239, 255, 255, 250);
	const uint16_t SSDP_PORT = 1900;

	String deviceUUID = this->app->computeUUID();

	// Send initial NOTIFY messages
	char notify[1024];

	// Send NOTIFY for different device types
	String deviceTypes[] = {
		"upnp:rootdevice",
		"urn:schemas-upnp-org:device:Basic:1",
		String("uuid:") + deviceUUID};

	const char *SSDP_NOTIFY_TEMPLATE = "NOTIFY * HTTP/1.1\r\n"
									   "HOST: 239.255.255.250:1900\r\n"
									   "CACHE-CONTROL: max-age=1800\r\n"
									   "LOCATION: http://%s:%d/description.xml\r\n"
									   "SERVER: SSDPServer/1.0\r\n"
									   "NT: %s\r\n"
									   "USN: uuid:%s::%s\r\n"
									   "NTS: ssdp:alive\r\n"
									   "BOOTID.UPNP.ORG: 1\r\n"
									   "CONFIGID.UPNP.ORG: 1\r\n"
									   "\r\n";

	for (int i = 0; i < 3; i++) {
		sprintf(notify, SSDP_NOTIFY_TEMPLATE,
			WiFi.localIP().toString().c_str(),
			this->wsport,
			deviceTypes[i].c_str(),
			deviceUUID.c_str(),
			deviceTypes[i].c_str());

		this->udp->beginPacket(SSDP_MULTICAST_ADDR, SSDP_PORT);
		this->udp->write((uint8_t *) notify, strlen(notify));
		this->udp->endPacket();
	}
}

void Webserver::announceSSDP() {
	INFO("Initializing SSDP...");

	const IPAddress SSDP_MULTICAST_ADDR(239, 255, 255, 250);
	const uint16_t SSDP_PORT = 1900;

	this->udp = new WiFiUDP();

	// Join multicast group for SSDP
	if (this->udp->beginMulticast(SSDP_MULTICAST_ADDR, SSDP_PORT)) {
		INFO("SSDP multicast joined successfully");
	} else {
		WARN("Failed to join SSDP multicast group");
	}
}

String Webserver::getSSDPDescription() {
	String deviceUUID = this->app->computeUUID();

	String xml = "<?xml version='1.0'?>";
	xml += "<root xmlns='urn:schemas-upnp-org:device-1-0'>";
	xml += "<specVersion><major>1</major><minor>0</minor></specVersion>";
	xml += "<device>";
	xml += "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>";
	xml += "<friendlyName>" + this->app->getName() + "</friendlyName>";
	xml += "<manufacturer>" + this->app->getManufacturer() + "</manufacturer>";
	xml += "<modelName>" + this->app->getDeviceType() + "</modelName>";
	xml += "<modelNumber>" + this->app->getVersion() + "</modelNumber>";
	xml += "<serialNumber>" + this->app->computeSerialNumber() + "</serialNumber>";
	xml += "<UDN>uuid:" + this->app->computeUUID() + "</UDN>";
	xml += "<presentationURL>" + getConfigurationURL() + "</presentationURL>";
	xml += "</device>";
	xml += "</root>";
	return xml;
}

void Webserver::begin() {
	this->announceMDNS();
	this->announceSSDP();

	this->server->config.max_uri_handlers = 30;
	this->server->listen(wsport);

	this->initialize();
}

void Webserver::loop() {
	static long lastSSDPNotify = millis();
	long now = millis();
	if (now - lastSSDPNotify > 5000) {
		this->ssdpNotify();
		lastSSDPNotify = now;
	}

	// SSDP

	// Incoming messages
	int packetSize = this->udp->parsePacket();
	if (packetSize) {
		char packetBuffer[512];
		int len = this->udp->read(packetBuffer, sizeof(packetBuffer) - 1);
		if (len > 0) {
			packetBuffer[len] = '\0';

			// Check if it's an M-SEARCH request
			if (strstr(packetBuffer, "M-SEARCH") && strstr(packetBuffer, "ssdp:discover")) {
				INFO("Received SSDP M-SEARCH request");

				// Extract search target
				char *stLine = strstr(packetBuffer, "ST:");
				String searchTarget = "upnp:rootdevice"; // Default

				if (stLine) {
					stLine += 3; // Skip "ST:"
					while (*stLine == ' ') {
						stLine++; // Skip spaces
					}
					char *end = strstr(stLine, "\r");
					if (end) {
						*end = '\0';
						searchTarget = String(stLine);
					}
				}

				char response[1024];
				char dateStr[64];

				// Simple date string (could be improved with real time)
				sprintf(dateStr, "Mon, 01 Jan 1970 00:00:00 GMT");

				String deviceUUID = this->app->computeUUID();

				const char *SSDP_RESPONSE_TEMPLATE = "HTTP/1.1 200 OK\r\n"
													 "CACHE-CONTROL: max-age=1800\r\n"
													 "DATE: %s\r\n"
													 "EXT:\r\n"
													 "LOCATION: http://%s:%d/description.xml\r\n"
													 "SERVER: SSDPServer/1.0\r\n"
													 "ST: %s\r\n"
													 "USN: uuid:%s::%s\r\n"
													 "BOOTID.UPNP.ORG: 1\r\n"
													 "CONFIGID.UPNP.ORG: 1\r\n"
													 "\r\n";

				sprintf(response, SSDP_RESPONSE_TEMPLATE,
					dateStr,
					WiFi.localIP().toString().c_str(),
					wsport,
					searchTarget.c_str(),
					deviceUUID.c_str(),
					searchTarget.c_str());

				// Send unicast response to requester
				this->udp->beginPacket(this->udp->remoteIP(), this->udp->remotePort());
				this->udp->write((uint8_t *) response, strlen(response));
				this->udp->endPacket();

				INFO("Sent SSDP response");
			}
		}
	}
	// Server runs async...
}
