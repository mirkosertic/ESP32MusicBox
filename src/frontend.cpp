#include "frontend.h"

#include <ArduinoJson.h>
#include <Ministache.h>
#include <WiFi.h>

#include "logging.h"

const char *INDEX_HTML = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{{appname}}</title>
  <style>
    /* Minimalistic but beautiful styles */
    body {
      font-family: 'Helvetica Neue', Arial, sans-serif;
      margin: 0;
      padding: 0;
      background-color: #f5f5f5;
      color: #333;
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      padding: 1rem;
    }
    
    h1 {
      font-size: 1.8rem;
      margin-bottom: 0.75rem;
    }
    
    .status-card, .file-browser, .rfid-status, .system-status, .player-controls {
      background-color: #fff;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
      padding: 0.75rem;
      margin-bottom: 1rem;
    }
    
    .status-card h2, .file-browser h2, .rfid-status h2, .system-status h2, .player-controls h2 {
      font-size: 1.3rem;
      margin-bottom: 0.5rem;
    }
    
    .file-list {
      list-style-type: none;
      padding: 0;
    }
    
    .file-list li {
      padding: 0.3rem 0;
      border-bottom: 1px solid #e6e6e6;
    }

    .file-list li:hover {
      background-color: #ddd;
    }      
    
    .file-list li:last-child {
      border-bottom: none;
    }
    
    .file-list li a {
      color: #333;
      text-decoration: none;
    }
    
    .file-list li a:hover {
      color: #666;
    }
    
    .directory-nav {
      display: flex;
      align-items: center;
      margin-bottom: 0.5rem;
    }
    
    .directory-nav a {
      color: #666;
      text-decoration: none;
      margin-right: 0.3rem;
    }
    
    .directory-nav a:hover {
      color: #333;
    }
    
    .rfid-tag {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0.3rem 0;
      border-bottom: 1px solid #e6e6e6;
    }
    
    .rfid-tag.active {
      background-color: #f0f8ff;
    }
    
    .rfid-tag:last-child {
      border-bottom: none;
    }
    
    .rfid-tag .tag-info {
      display: flex;
      align-items: center;
    }
    
    .rfid-tag .tag-info span {
      margin-left: 0.3rem;
    }
    
    .rfid-tag .tag-actions button {
      background-color: #007bff;
      color: #fff;
      border: none;
      padding: 0.3rem 0.6rem;
      border-radius: 4px;
      cursor: pointer;
      margin-left: 0.3rem;
    }
    
    .rfid-tag .tag-actions button:hover {
      background-color: #0056b3;
    }
    
    .rfid-tag .tag-actions button.disabled {
      background-color: #ccc;
      cursor: not-allowed;
    }
    
    .rfid-tag .tag-actions button.delete {
      background-color: #dc3545;
    }
    
    .rfid-tag .tag-actions button.delete:hover {
      background-color: #c82333;
    }
    
    .system-status {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    
    .system-status .status-item {
      display: flex;
      align-items: center;
    }
    
    .system-status .status-item span {
      margin-left: 0.3rem;
    }
    
    .system-status .status-icon {
      font-size: 1.2rem;
    }
    
    .player-controls {
      display: flex;
      justify-content: center;
      align-items: center;
    }
    
    .player-controls button {
      background-color: #007bff;
      color: #fff;
      border: none;
      padding: 0.4rem 0.8rem;
      border-radius: 4px;
      cursor: pointer;
      margin: 0 0.3rem;
    }
    
    .player-controls button:hover {
      background-color: #0056b3;
    }

    .volume-control {
        display: flex;
        text-align: center;
        align-items: center;
        gap: 0.5rem;
    }

    input[type="range"] {
        max-width: 100%;
        margin: 10px 0;
        width: 20rem;
    }

    output {
        font-size: 18px;
    }

    a {
      color: black;
      text-decoration: none;		
    }

    a:visited {
      color: black;
      text-decoration: none;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>{{appname}}</h1>
    
    <div class="status-card">
      <h2>Player Status</h2>
      <p>Current song: {{songinfo}}
      <div class="volume-control">
        Volume: <input type="range" id="volume" min="0" max="100" value="{{volume}}" step="1"> <output for="volume">{{volume}} %</output>
      </div>      
      <br>Playback: {{playingstatus}}</p>
    </div>
    
    <div class="player-controls">
      <a href="/startstop?path={{basepath}}"><button>&#x23F8; Start/Stop</button></a>
      <a href="/previous?path={{basepath}}"><button>&#x23ED; Previous Title</button></a>
      <a href="/next?path={{basepath}}"><button>&#x23EE; Next Title</button></a>
    </div>

    <div class="rfid-status">
      <h2>RFID Tag Scanner</h2>
      {{#tagscanner}}
      <div class="rfid-tag active">
        <div class="tag-info">
          <span class="tag-icon">&#x1F4E6;</span>
          <span>{{tagname}} {{info}}</span>
        </div>
        <div class="tag-actions">
          <a href="/assign?path={{basepath}}"><button>Assign Tag to current directory</button></a>
          <a href="/delete?path={{basepath}}"><button class="delete">Clear Tag</button></a>
        </div>
      </div>
      {{/tagscanner}}
    </div>

    <div class="file-browser">
      <h2>File Browser</h2>
      <div class="directory-nav">
        {{#parent}}<a href="?path={{parent}}">&#128281;{{parent}}</a>{{/parent}}
        <span>{{base}}</span>
      </div>
      <ul class="file-list">
        {{#files}}
        {{#dirname}}
        <li><a href="/?path={{path}}">&#x1F4C1; {{dirname}}</a></li>
        {{/dirname}}
        {{#filename}}
        <li><a href="/play?path={{basepath}}&index={{index}}">&#x1F50A;{{filename}}</a></li>
        {{/filename}}
        {{/files}}
      </ul>
    </div>

    <div class="system-status">
      <div class="status-item">
        <span class="status-icon">&#x1F4F6;</span>
        <a href="/networks.html"><span>WIFI: {{wifistatus}}</span></a>
      </div>
      <div class="status-item">
        <span class="status-icon">&#9881;</span>      
        <a href="/settings.html"><span>Device Configuration</span></a>      
      <div>
    </div>
  </div>
  <script>
      (function() {
        const volumeSlider = document.getElementById('volume');
        const output = document.querySelector('output');

        function updateVolume(event) {
            output.textContent = event.target.value + " %";
        };

        function updateVolumeChange(event) {
            window.location.href='/volume?path={{basepath}}&volume=' + event.target.value;
        };

        volumeSlider.addEventListener('input', updateVolume);
        volumeSlider.addEventListener('change', updateVolumeChange);

        function checkStateVersion(url, currentversion) {
          const xhr = new XMLHttpRequest();
          
          xhr.open('HEAD', url, true);
          
          xhr.onload = function() {
            if (xhr.status >= 200 && xhr.status < 300) {
              const stateVersion = xhr.getResponseHeader('x-stateversion');
              
              if (stateVersion > currentversion) {
                window.location.reload(true);
              }
            }
          };
          
          xhr.send();
        };

        setInterval(() => checkStateVersion("/", {{stateversion}}), 500);
      })();
  </script>
</body>
</html>
)";

const char *NETWORKS_HTML = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{{appname}} - WLAN Networks</title>
  <style>
    /* Minimalistic but beautiful styles */
    body {
      font-family: 'Helvetica Neue', Arial, sans-serif;
      margin: 0;
      padding: 0;
      background-color: #f5f5f5;
      color: #333;
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      padding: 1rem;
    }
    
    h1 {
      font-size: 1.8rem;
      margin-bottom: 0.75rem;
    }
    
    .status-card, .file-browser, .rfid-status, .system-status, .player-controls {
      background-color: #fff;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
      padding: 0.75rem;
      margin-bottom: 1rem;
    }
    
    .status-card h2, .file-browser h2, .rfid-status h2, .system-status h2, .player-controls h2 {
      font-size: 1.3rem;
      margin-bottom: 0.5rem;
    }

    table {
        width: 100%;
        border-collapse: collapse;
        background-color: white;
        box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    th, td {
        padding: 12px;
        text-align: left;
        border-bottom: 1px solid #ddd;
    }
    th {
        background-color: black;
        color: white;
    }
    xtr:nth-child(even) {
        background-color: #f2f2f2;
    }
    tr:hover {
        background-color: #ddd;
    }    
   
    .system-status {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    
    .system-status .status-item {
      display: flex;
      align-items: center;
    }
    
    .system-status .status-item span {
      margin-left: 0.3rem;
    }
    
    .system-status .status-icon {
      font-size: 1.2rem;
    }
    
    a {
      color: black;
      text-decoration: none;		
    }

    a:visited {
      color: black;
      text-decoration: none;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>{{appname}} - WLAN Networks</h1>
    
    <div class="status-card">
      <h2>Network List</h2>
      <br>
      <table>
        <tr>
          <th>SSID</th>
          <th>RSSI(dBm)</th>
          <th>Channel</th>
          <th>Encryption</th>
          <th>BSSID</th>
        </tr>
        {{#networks}}
        <tr>
          <td>{{ssid}}</td>
          <td>{{rssi}} ({{rssitext}})</td>
          <td>{{channel}}</td>
          <td>{{encryption}}</td>
          <td>{{bssid}}</td>
        </tr>
        {{/networks}}    
      </table> 
    </div>
    
    <div class="system-status">
      <div class="status-item">
        <span class="status-icon">&#x1F4F6;</span>
        <a href="/networks.html"><span>WIFI: {{wifistatus}}</span></a>
      </div>
      <div class="status-item">
        <span class="status-icon">&#9881;</span>      
        <a href="/settings.html"><span>Device Configuration</span></a>      
      <div>
    </div>
  </div>
</body>
</html>
)";

const char *SETTINGS_HTML = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{{appname}} - Settings</title>
  <style>
    /* Minimalistic but beautiful styles */
    body {
      font-family: 'Helvetica Neue', Arial, sans-serif;
      margin: 0;
      padding: 0;
      background-color: #f5f5f5;
      color: #333;
    }
    
    .container {
      max-width: 800px;
      margin: 0 auto;
      padding: 1rem;
    }
    
    h1 {
      font-size: 1.8rem;
      margin-bottom: 0.75rem;
    }
    
    .status-card, .file-browser, .rfid-status, .system-status, .player-controls {
      background-color: #fff;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
      padding: 0.75rem;
      margin-bottom: 1rem;
    }
    
    .status-card h2, .file-browser h2, .rfid-status h2, .system-status h2, .player-controls h2 {
      font-size: 1.3rem;
      margin-bottom: 0.5rem;
    }

    .system-status {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    
    .system-status .status-item {
      display: flex;
      align-items: center;
    }
    
    .system-status .status-item span {
      margin-left: 0.3rem;
    }
    
    .system-status .status-icon {
      font-size: 1.2rem;
    }
    
    textarea {
      width: 100%;
      height: 22rem;
    }
    
    a {
      color: black;
      text-decoration: none;		
    }

    a:visited {
      color: black;
      text-decoration: none;
    }

    #submit-button {
        background-color: red;
        color: white;
        padding: 10px 20px;
        border: none;
        cursor: pointer;
        margin-top: 10px;
    }
    #error-message {
        color: red;
        margin-top: 10px;
    }	
  </style>
</head>
<body>
  <div class="container">
    <h1>{{appname}} - Settings</h1>
    
    <div class="status-card">
      <h2>Settings as JSON</h2>
      <form id="json-form" onsubmit="return validateAndSubmit();">
        <textarea id="editor">{{settingsjson}}</textarea>
        <div id="error-message"></div>
        <button type="submit" id="submit-button">!! Save !!</button>
      </form>	        
    </div>
    
    <script>
      function validateAndSubmit() {
          var jsonString = document.getElementById('editor').value;
          try {
              JSON.parse(jsonString);
              document.getElementById('error-message').textContent = '';
              console.log('Form submitted with valid JSON:', jsonString);
              // Here you would typically send the data to a server
              return false;
          } catch (e) {
              console.log('Form submitted with valid JSON:', jsonString);
              document.getElementById('error-message').textContent = 'Invalid JSON: ' + e.message;
              return false;
          }
      }	
    </script>

    <div class="system-status">
      <div class="status-item">
        <span class="status-icon">&#x1F4F6;</span>
        <a href="/networks.html"><span>WIFI: {{wifistatus}}</span></a>
      </div>
      <div class="status-item">
        <span class="status-icon">&#9881;</span>      
        <a href="/settings.html"><span>Device Configuration</span></a>      
      <div>
    </div>
  </div>
</body>
</html>
)";

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

    const String output = ministache::render(INDEX_HTML, result);

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
    esp_task_wdt_init(30, true); // 30 Sekunden Timeout
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

    const String output = ministache::render(NETWORKS_HTML, result);
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", output);
    response->addHeader("Cache-Control","no-cache, must-revalidate");
    request->send(response); });

  this->server->on("/settings.html", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {

    JsonDocument result;

    result["appname"] = this->app->getName();
    result["wifistatus"] = categorizeRSSI(WiFi.RSSI());    
    result["settingsjson"] = this->settings->getSettingsAsJson();    

    const String output = ministache::render(SETTINGS_HTML, result);
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", output);
    response->addHeader("Cache-Control","no-cache, must-revalidate");
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

                     request->send(200, "text/xml", app->getSSDPSchema());
                   });

  this->server->onNotFound([&](AsyncWebServerRequest *request)
                           {
              INFO_VAR("Not Found : %s Method %s", request->url().c_str(), request->methodToString());
              request->send(404, "text/plain", "Not found"); });
}

void Frontend::begin()
{
  this->server->begin();
}
