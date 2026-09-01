#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "ct_server.h"
#include "ct_hardware.h"   
#include "ct_persistence.h"
#include "ct_automation.h"

WebServer server(80); 

int targetPercent = 0;
unsigned long lastConnectionCheckTime = 0;
unsigned long disconnectWindowStart = 0;
int disconnectCounter               = 0;
const int maxDisconnectAllowed      = 5; 

unsigned long trackingTimeLimit       = 300000;
unsigned long connectionCheckInterval = 60000; 

bool ledState = false;
bool displayConnectedTime = true;
bool enableDebug = false;

bool isConfigPortalActive = false;
unsigned long lastActiveIPTime = 0;

bool isPendingDirectionFlip = false;
bool pendingDirection       = true;

static volatile bool isProcessingDisconnect = false; 

extern int targetSpeed;
extern int storedRunSpeed;
extern int currentSpeed;
extern bool isForward;
extern TrainState currentState;
extern volatile TrainConfig config;

extern bool irTrippedActiveStop;

void handleRootDashboard() {
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    server.sendHeader("Cache-Control", "public, max-age=31536000");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "Critical Error: index.html not found inside the flash memory partition.");
  }
}

void handleStatusUpdate() {
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
  json += "\"target\":" + String(targetPercent);     
  json += "}";
  server.send(200, "application/json", json);
}

void handleSaveSelectPercent() {
  if (server.hasArg("val")) {
    targetPercent = server.arg("val").toInt();
  }
  server.send(200, "text/plain", "Percentage Stored");
}

void handleSetLed() {
  if (server.hasArg("state")) {
    ledState = (server.arg("state").toInt() == 1);
    switchOnboardLED(ledState);
  }
  server.send(200, "text/plain", "LED Updated");
}

void handleSaveDefaultSpeed() {
  config.defaultSpeed = targetPercent; 
  saveTrainConfigToFlash(); 
  server.send(200, "text/plain", "Default Speed Saved to Flash");
}

void handleStartTrain() {
  if (currentState == EMERGENCY_STOP || currentState == STOPPED || currentState == AT_STATION) {
    currentState = RAMPING_UP; 
  }
  targetSpeed = map(targetPercent, 0, 100, 0, 220); 
  storedRunSpeed = targetSpeed; 
  server.send(200, "text/plain", "Started");
}

void handleSmoothStop() {
  if (currentState == RUNNING || currentState == RAMPING_UP) {
    currentState = RAMPING_DOWN; 
  }
  irTrippedActiveStop = false; 
  targetSpeed = 0; 
  server.send(200, "text/plain", "Smooth Stop Active");
}

void handleSetDirection() {
  if (server.hasArg("dir")) {
    bool targetDir = (server.arg("dir") == "forward");
    
    if (currentState == EMERGENCY_STOP) {
      currentState = RUNNING;
    }

    if (currentSpeed > 0 && isForward != targetDir) {
      isPendingDirectionFlip = true;
      pendingDirection = targetDir;
      targetSpeed = 0; 
    } else {
      isForward = targetDir;
      targetSpeed = storedRunSpeed;
      applyTrackPower();
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleUpdatePhysics() {
  if (server.hasArg("step") && server.hasArg("interval") && server.hasArg("wait") && server.hasArg("cooldown") && server.hasArg("clamp")) {
    config.rampStep = server.arg("step").toInt();
    config.rampInterval = server.arg("interval").toInt();
    config.stationWaitDuration = server.arg("wait").toInt();
    config.irCooldown = server.arg("cooldown").toInt();
    config.minSpeedClamp = server.arg("clamp").toInt();
    saveTrainConfigToFlash(); 
  }
  server.send(200, "text/plain", "Saved");
}

void handleEmergencyStop() {
  targetSpeed = 0;
  storedRunSpeed = 0;
  currentSpeed = 0; 
  currentState = EMERGENCY_STOP; 
  applyTrackPower();
  server.send(200, "text/plain", "Emergency Stop Executed");
}

void handleSetLedMode() {
  server.send(200, "text/plain", "Muted");
}

void handleSetDebug() {
  if (server.hasArg("state")) {
    enableDebug = (server.arg("state").toInt() == 1);
    enableConnectedTime(enableDebug);
    Serial.printf("[%lu ms] System state parameters shifted: enableDebug = %s\n", millis(), enableDebug ? "TRUE" : "FALSE");
  }
  server.send(200, "text/plain", "Debug Target State Synced");
}

void handleClearFlash() {
  if (server.hasArg("confirm") && server.arg("confirm") == "true") {
    Serial.printf("[%lu ms] FACTORY RESET INITIALIZED: Wiping local configuration blocks...\n", millis());
    enableConnectedTime(false);
    setOLEDLine1("FACTORY");
    setOLEDLine2("RESET   ");
    
    Preferences prefs;
    prefs.begin("train-core", false);
    prefs.clear(); 
    prefs.end();
    
    Serial.printf("[%lu ms] NVRAM wiped cleanly. Executing immediate software restart...\n", millis());
    server.send(200, "text/plain", "Flash partitions cleared. Controller is resetting to factory default settings...");
    delay(2000); 
    ESP.restart(); 
  } else {
    if (LittleFS.exists("/reset.html")) {
      File file = LittleFS.open("/reset.html", "r");
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.send(404, "text/plain", "Error: reset.html warning panel asset missing from Flash storage.");
    }
  }
}

void initServer() {
  isProcessingDisconnect = true; 
  targetPercent = config.defaultSpeed;

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

    server.on("/", HTTP_GET, handleRootDashboard);
  }

  server.on("/status", HTTP_GET, handleStatusUpdate);
  server.on("/saveselectpercent", HTTP_GET, handleSaveSelectPercent);
  server.on("/setled", HTTP_GET, handleSetLed);
  server.on("/savedefaultspeed", HTTP_GET, handleSaveDefaultSpeed);
  server.on("/start", HTTP_GET, handleStartTrain);
  server.on("/smoothstop", HTTP_GET, handleSmoothStop);
  server.on("/setdir", HTTP_GET, handleSetDirection);
  server.on("/updatephysics", HTTP_GET, handleUpdatePhysics);
  server.on("/stop", HTTP_GET, handleEmergencyStop);
  server.on("/setledmode", HTTP_GET, handleSetLedMode);
  server.on("/setdebug", HTTP_GET, handleSetDebug);
  server.on("/clearflash", HTTP_GET, handleClearFlash);
  
  server.begin();
  isProcessingDisconnect = false; 
}

void handlePortalRoot() {
  if (LittleFS.exists("/portal.html")) {
    File file = LittleFS.open("/portal.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "Error: portal.html configuration asset missing from Flash storage.");
  }
}

void handlePortalSaveWifi() {
  if (server.hasArg("s")) {
    String newSSID = server.arg("s");
    String newPASS = server.hasArg("p") ? server.arg("p") : "";
    
    memset((void*)&config.wifiSSID, 0, sizeof(config.wifiSSID));
    memset((void*)&config.wifiPASS, 0, sizeof(config.wifiPASS));
    strncpy((char*)&config.wifiSSID, newSSID.c_str(), sizeof(config.wifiSSID) - 1);
    strncpy((char*)&config.wifiPASS, newPASS.c_str(), sizeof(config.wifiPASS) - 1);
    
    saveTrainConfigToFlash(); 
    
    Serial.printf("[%lu ms] New credentials saved to Flash. Executing orderly clean software reset...\n", millis());
    
    server.send(200, "text/plain", "Credentials Saved. Rebooting train layout...");
    delay(2000);
    ESP.restart(); 
  } else {
    server.send(400, "text/plain", "Bad Request");
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

  server.on("/", HTTP_GET, handlePortalRoot);
  server.on("/savewifi", HTTP_GET, handlePortalSaveWifi);
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
  setOLEDLine2(disconnBuffer, 1);
    
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

     if (displayConnectedTime && lastActiveIPTime > 0 && currentSpeed == 0) {
      unsigned long totalSeconds = (currentTime - lastActiveIPTime) / 1000;
      
      unsigned int seconds = totalSeconds % 60;
      unsigned int minutes = (totalSeconds / 60) % 60;
      unsigned int hours   = totalSeconds / 3600; 

      char uptimeBuffer[DISPLAY_BUFFER_SIZE];
      snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%02uh%02um%02us", hours, minutes, seconds);
      
      setOLEDLine2(uptimeBuffer);
    }
  }
}


