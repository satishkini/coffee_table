#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "ct_server.h"
#include "ct_hardware.h"   
#include "ct_persistence.h"
#include "ct_automation.h"
#include "ct_index.h"

AsyncWebServer server(80); 


extern int targetSpeed;
extern int storedRunSpeed;
extern int currentSpeed;
extern bool isForward;
extern TrainState currentState;

extern unsigned long trackingTimeLimit;
extern volatile TrainConfig config;


const int maxDisconnectAllowed      = 5; 
unsigned long connectionCheckInterval = 60000;  
unsigned long trackingTimeLimit       = 300000;

bool ledState = false;
bool displayConnectedTime = true;
bool enableDebug = false;

bool isConfigPortalActive = false;
unsigned long lastActiveIPTime = 0;
extern bool irTrippedActiveStop; 

int targetPercent = 0;
unsigned long lastConnectionCheckTime = 0;
unsigned long disconnectWindowStart = 0;
int disconnectCounter               = 0;
volatile bool isProcessingDisconnect = false;

bool isPendingDirectionFlip = false;
bool pendingDirection = true;


void handleRootDashboard(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "");
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  request->send_P(200, "text/html", HTML_PAGE); 
}

void handleStatusUpdate(AsyncWebServerRequest *request) {
  TrainConfig snap;
  snap.rampStep             = config.rampStep;
  snap.rampInterval         = config.rampInterval;
  snap.stationWaitDuration  = config.stationWaitDuration;
  snap.irCooldown           = config.irCooldown;
  snap.minSpeedClamp        = config.minSpeedClamp;

  String stateStr = "RUNNING";
  if (currentState == STOPPED)            stateStr = "STOPPED";
  else if (currentState == AT_STATION)    stateStr = "AT STATION";
  else if (currentState == RAMPING_UP)    stateStr = "RAMPING UP";   
  else if (currentState == RAMPING_DOWN)  stateStr = "RAMPING DOWN"; 
  else if (currentState == EMERGENCY_STOP) stateStr = "EMERGENCY STOP";

  int livePercent = map(currentSpeed, 0, 220, 0, 100);
  if (currentSpeed == 0) livePercent = 0; 

  String json = "{";
  json += "\"ledState\":" + String(ledState ? 1 : 0) + ","; 
  json += "\"dir\":\"" + String(isForward ? "forward" : "reverse") + "\",";
  json += "\"step\":" + String(snap.rampStep) + ",";
  json += "\"interval\":" + String(snap.rampInterval) + ",";
  json += "\"wait\":" + String(snap.stationWaitDuration) + ",";
  json += "\"cooldown\":" + String(snap.irCooldown) + ",";
  json += "\"clamp\":" + String(snap.minSpeedClamp) + ",";
  json += "\"debug\":" + String(enableDebug ? 1 : 0) + ",";
  json += "\"state\":\"" + stateStr + "\",";       
  json += "\"current\":" + String(livePercent) + ",";
  json += "\"target\":" + String(targetPercent);     // NEW JSON ENTRY: Passes current target percentage block back to UI
  json += "}";
  request->send(200, "application/json", json);
}


void handleSaveSelectPercent(AsyncWebServerRequest *request) {
  if (request->hasArg("val")) {
    targetPercent = request->arg("val").toInt();
  }
  request->send(200, "text/plain", "Percentage Stored");
}

void handleSetLed(AsyncWebServerRequest *request) {
  if (request->hasArg("state")) {
    ledState = (request->arg("state").toInt() == 1);
    switchOnboardLED(ledState);
  }
  request->send(200, "text/plain", "LED Updated");
}

void handleStartTrain(AsyncWebServerRequest *request) {
  if (currentState == EMERGENCY_STOP || currentState == STOPPED || currentState == AT_STATION) {
    currentState = RAMPING_UP; // Directly triggers RAMPING_UP transition
  }
  targetSpeed = map(targetPercent, 0, 100, 0, 220); 
  storedRunSpeed = targetSpeed; 
  request->send(200, "text/plain", "Started");
}

void handleSmoothStop(AsyncWebServerRequest *request) {
  if (currentState == RUNNING || currentState == RAMPING_UP) {
    currentState = RAMPING_DOWN; // Directly triggers RAMPING_DOWN transition
  }
  irTrippedActiveStop = false; 
  targetSpeed = 0; 
  request->send(200, "text/plain", "Smooth Stop Active");
}

void handleSetDirection(AsyncWebServerRequest *request) {
  if (request->hasArg("dir")) {
    bool targetDir = (request->arg("dir") == "forward");
    
    if (currentSpeed > 0 && isForward != targetDir) {
      // INTERLOCK: Mark that we need to flip directions, and command a smooth stop first
      isPendingDirectionFlip = true;
      pendingDirection = targetDir;
      targetSpeed = 0; 
      
      if (enableDebug) {
        Serial.printf("[%lu ms] DEBUG: Direction change requested. Ramping down train safely first...\n", millis());
      }
    } else {
      // If the train is already stopped or moving in the same direction, apply it instantly
      isForward = targetDir;
      targetSpeed = storedRunSpeed;
      applyTrackPower();
    }
  }
  request->send(200, "text/plain", "OK");
}

void handleUpdatePhysics(AsyncWebServerRequest *request) {
  if (request->hasArg("step") && request->hasArg("interval") && request->hasArg("wait") && request->hasArg("cooldown") && request->hasArg("clamp")) {
    config.rampStep = request->arg("step").toInt();
    config.rampInterval = request->arg("interval").toInt();
    config.stationWaitDuration = request->arg("wait").toInt();
    config.irCooldown = request->arg("cooldown").toInt();
    config.minSpeedClamp = request->arg("clamp").toInt();
    saveTrainConfigToFlash(); 
  }
  request->send(200, "text/plain", "Saved");
}

void handleEmergencyStop(AsyncWebServerRequest *request) {
  targetSpeed = 0;
  storedRunSpeed = 0;
  currentSpeed = 0; 
  currentState = EMERGENCY_STOP; 
  applyTrackPower();
  request->send(200, "text/plain", "Emergency Stop Executed");
}

void handleSetLedMode(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Muted");
}

void handleSetDebug(AsyncWebServerRequest *request) {
  if (request->hasArg("state")) {
    enableDebug = (request->arg("state").toInt() == 1);
    enableConnectedTime(enableDebug);
    Serial.printf("[%lu ms] System state parameters shifted: enableDebug = %s\n", millis(), enableDebug ? "TRUE" : "FALSE");
  }
  request->send(200, "text/plain", "Debug Target State Synced");
}

void handleClearFlash(AsyncWebServerRequest *request) {
  if (request->hasArg("confirm") && request->arg("confirm") == "true") {
    Serial.printf("[%lu ms] FACTORY RESET INITIALIZED: Wiping local configuration blocks...\n", millis());
    enableConnectedTime(false);
    setOLEDLine1("FACTORY");
    setOLEDLine2("RESET   ");
    
    Preferences prefs;
    prefs.begin("train-core", false);
    prefs.clear(); 
    prefs.end();
    
    Serial.printf("[%lu ms] NVRAM wiped cleanly. Executing immediate software restart...\n", millis());
    request->send(200, "text/plain", "Flash partitions cleared. Controller is resetting to factory default settings...");
    delay(2000); 
    ESP.restart(); 
  } else {
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    html += "<style>body{font-family:Arial;background:#111;color:#fff;text-align:center;padding:30px;}";
    html += ".box{max-width:350px;margin:40px auto;background:#222;padding:25px;border-radius:12px;border:2px solid #cc0000;}";
    html += "h3{color:#cc0000;margin-top:0;} p{color:#aaa;font-size:14px;line-height:1.5;}";
    html += ".btn{display:inline-block;background:#cc0000;color:#fff;text-decoration:none;padding:12px 25px;font-weight:bold;border-radius:6px;margin:10px 5px;cursor:pointer;}";
    html += ".btn-cancel{background:#444;color:#eee;}</style></head><body>";
    html += "<div class='box'><h3>Warning: Factory Reset</h3>";
    html += "<p>This action will permanently erase your saved Wi-Fi networks, physics parameters, and custom configuration registers.</p>";
    html += "<a href='/clearflash?confirm=true' class='btn'>CONFIRM HARD WIPE</a>";
    html += "<a href='/' class='btn btn-cancel'>CANCEL</a></div></body></html>";
    request->send(200, "text/html", html);
  }
}

void handleSaveDefaultSpeed(AsyncWebServerRequest *request) {
  config.defaultSpeed = targetPercent; 
  saveTrainConfigToFlash(); 
  request->send(200, "text/plain", "Default Speed Saved to Flash");
}

void initServer() {
  isProcessingDisconnect = true; 

  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA); 
  
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setHostname("Coffee-Table"); 

  setOLEDLine1("CONNECTING");
  WiFi.begin((const char*)config.wifiSSID, (const char*)config.wifiPASS);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    bootConfigPortal(); 
  } else {
    Serial.printf("[%lu ms] Network ready. IP Address: ", millis());
    Serial.println(WiFi.localIP());
    setOLEDLine1("ONLINE");

    server.on("/", WebRequestMethod::HTTP_GET, handleRootDashboard);
  }

  server.on("/status", WebRequestMethod::HTTP_GET, handleStatusUpdate);
  server.on("/saveselectpercent", WebRequestMethod::HTTP_GET, handleSaveSelectPercent);
  server.on("/setled", WebRequestMethod::HTTP_GET, handleSetLed);
  server.on("/start", WebRequestMethod::HTTP_GET, handleStartTrain);
  server.on("/smoothstop", WebRequestMethod::HTTP_GET, handleSmoothStop);
  server.on("/setdir", WebRequestMethod::HTTP_GET, handleSetDirection);
  server.on("/updatephysics", WebRequestMethod::HTTP_GET, handleUpdatePhysics);
  server.on("/stop", WebRequestMethod::HTTP_GET, handleEmergencyStop);
  server.on("/setledmode", WebRequestMethod::HTTP_GET, handleSetLedMode);
  server.on("/setdebug", WebRequestMethod::HTTP_GET, handleSetDebug);
  server.on("/clearflash", WebRequestMethod::HTTP_GET, handleClearFlash);
  server.on("/savedefaultspeed", WebRequestMethod::HTTP_GET, handleSaveDefaultSpeed);
  
  server.begin();
  isProcessingDisconnect = false; 
}

void handlePortalRoot(AsyncWebServerRequest *request) {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<style>body{font-family:Arial;background:#111;color:#fff;text-align:center;padding:20px;}";
  html += "input{display:block;width:80%;max-width:300px;margin:15px auto;padding:12px;background:#222;border:1px solid #444;color:#fff;border-radius:6px;}";
  html += "button{background:#00adb5;color:#fff;border:none;padding:12px 30px;font-size:16px;font-weight:bold;border-radius:6px;cursor:pointer;}</style>";
  html += "</head><body><h2>WiFi Setup Portal</h2><form action='/savewifi' method='GET'>";
  html += "<input type='text' name='s' placeholder='WiFi SSID' required>";
  html += "<input type='password' name='p' placeholder='WiFi Password'>";
  html += "<button type='submit'>SAVE AND CONNECT</button></form></body></html>";
  request->send(200, "text/html", html);
}

void handlePortalSaveWifi(AsyncWebServerRequest *request) {
  if (request->hasArg("s")) {
    String newSSID = request->arg("s");
    String newPASS = request->hasArg("p") ? request->arg("p") : "";
    
    memset((void*)&config.wifiSSID, 0, sizeof(config.wifiSSID));
    memset((void*)&config.wifiPASS, 0, sizeof(config.wifiPASS));
    strncpy((char*)&config.wifiSSID, newSSID.c_str(), sizeof(config.wifiSSID) - 1);
    strncpy((char*)&config.wifiPASS, newPASS.c_str(), sizeof(config.wifiPASS) - 1);
    
    saveTrainConfigToFlash(); 
    
    Serial.printf("[%lu ms] New credentials saved to Flash. Executing orderly clean software reset...\n", millis());
    
    request->send(200, "text/plain", "Credentials Saved. Rebooting train layout...");
    delay(2000);
    ESP.restart(); 
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
}

void bootConfigPortal() {
  Serial.printf("[%lu ms] Handshake failed. Booting async local setup AP...\n", millis());
  setOLEDLine1("NET FAIL");
  delay(1000);
  
  isConfigPortalActive = true; 
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Coffee-Table"); 
  
  Serial.printf("[%lu ms] Configuration Portal Active. Open IP: ", millis());
  Serial.println(WiFi.softAPIP());
  setOLEDLine1("LOCAL AP");

  Serial.println("[AP Mode] Waiting for a client device to connect to 'Coffee-Table-Train'...");
  while (WiFi.softAPgetStationNum() == 0) {
    delay(1000); 
      
    static bool toggle = false;
    toggle = !toggle;
    setOLEDLine1(toggle ? "PAIR PHONE" : "LOCAL AP");
  }

  server.on("/", WebRequestMethod::HTTP_GET, handlePortalRoot);
  server.on("/savewifi", WebRequestMethod::HTTP_GET, handlePortalSaveWifi);
}

void processConnectionCheck(unsigned long currentTime) {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return; 
  }

  if (currentTime - lastConnectionCheckTime >= connectionCheckInterval) {
    lastConnectionCheckTime = currentTime;
    
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() == WIFI_STA) {
      Serial.printf("[%lu ms] System still offline. Issuing active 60s retry sweep...\n", currentTime);
      
      enableConnectedTime(false);
      setOLEDLine1("DISCONN");
      
      handleNetworkDisconnections(currentTime);
    } else if (WiFi.status() == WL_CONNECTED) {
      isProcessingDisconnect = false;
      disconnectCounter = 0;
    }
  }
}

void handleNetworkDisconnections(unsigned long currentTime) {
  isProcessingDisconnect = true;
  setOLEDLine1("RECONN");
  WiFi.reconnect(); 

  if (disconnectCounter == 0) {
    disconnectWindowStart = currentTime;
  }
  disconnectCounter++;

  if (currentTime - disconnectWindowStart > trackingTimeLimit) {
    disconnectCounter = 1;
    disconnectWindowStart = currentTime;
  }
  
  char disconnBuffer[DISPLAY_BUFFER_SIZE];
  snprintf(disconnBuffer, DISPLAY_BUFFER_SIZE , "DISCONN:%02u", disconnectCounter);
  setOLEDLine2(disconnBuffer);
    
  if (disconnectCounter >= maxDisconnectAllowed) {
    Serial.printf("[%lu ms] WATCHDOG THRESHOLD REACHED (%d failures). Executing clean emergency software reset...\n", millis(), disconnectCounter);
    setOLEDLine1("NET WDT");
    enableConnectedTime(false);
    delay(1000);
    ESP.restart();
  }
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  unsigned long now = millis();
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[%lu ms] Obtained stable link allocation address via Pi-hole: ", now);
      Serial.println(WiFi.localIP());
      isProcessingDisconnect = false;
      setOLEDLine1("ONLINE");
      setOLEDLine2("");
      enableConnectedTime(true);
      lastActiveIPTime = now;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (!isProcessingDisconnect && WiFi.getMode() == WIFI_STA) {
        uint8_t reasonCode = info.wifi_sta_disconnected.reason;
        Serial.printf("[%lu ms] Asynchronous hardware drop frame. Reason Code: %u\n", now, reasonCode);
        
        enableConnectedTime(false);
        
        char oledReasonBuffer[DISPLAY_BUFFER_SIZE];
        snprintf(oledReasonBuffer, sizeof(oledReasonBuffer), "LL:%u", reasonCode);
        setOLEDLine1(oledReasonBuffer);
        
        isProcessingDisconnect = true; 
      }
      break;
    default:
      break;
  }
}

bool isNetworkLinkStable() {
  return (WiFi.status() == WL_CONNECTED && !isProcessingDisconnect);
}

void enableConnectedTime(bool enable) {
  displayConnectedTime = enable;
  if (!displayConnectedTime) {
    setOLEDLine2("");
  }
}

void processOnlineTime(unsigned long currentTime) {
  static unsigned long lastUptimeRefresh = 0;

  if (!enableDebug) {
    return;
  }

  if (currentTime - lastUptimeRefresh >= 1000) {
    lastUptimeRefresh = currentTime;

     if (displayConnectedTime && lastActiveIPTime > 0) {
      unsigned long totalSeconds = (currentTime - lastActiveIPTime) / 1000;
      unsigned int seconds = totalSeconds % 60;
      unsigned int minutes = (totalSeconds / 60) % 60;
      unsigned int hours   = (totalSeconds / 3600);

      char uptimeBuffer[11];

      snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%02uh%02um%02us", hours, minutes, seconds);
      
      setOLEDLine2(uptimeBuffer);
    }
  }
}
