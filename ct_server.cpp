#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "ct_server.h"
#include "ct_hardware.h"   
#include "ct_persistence.h"
#include "ct_automation.h"
#include "ct_index.h"

AsyncWebServer server(80); 

int targetPercent = 0;
unsigned long lastConnectionCheckTime = 0;
unsigned long disconnectWindowStart = 0;
int disconnectCounter               = 0;
const int maxDisconnectAllowed      = 5; 

extern int targetSpeed;
extern int storedRunSpeed;
extern int currentSpeed;
extern bool isForward;
extern TrainState currentState;

extern unsigned long trackingTimeLimit;
volatile bool isProcessingDisconnect = false;
extern volatile TrainConfig config;

bool ledState = false;
bool displayConnectedTime = true;
bool enableDebug = false;

bool isConfigPortalActive = false;
unsigned long lastActiveIPTime = 0;

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

    server.on("/", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "");
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send_P(200, "text/html", HTML_PAGE); 
    });
  }

  server.on("/status", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    TrainConfig snap;
    snap.rampStep             = config.rampStep;
    snap.rampInterval         = config.rampInterval;
    snap.stationWaitDuration  = config.stationWaitDuration;
    snap.irCooldown           = config.irCooldown;
    snap.minSpeedClamp       = config.minSpeedClamp;

    String json = "{";
    json += "\"ledState\":" + String(ledState ? 1 : 0) + ","; 
    json += "\"dir\":\"" + String(isForward ? "forward" : "reverse") + "\",";
    json += "\"step\":" + String(snap.rampStep) + ",";
    json += "\"interval\":" + String(snap.rampInterval) + ",";
    json += "\"wait\":" + String(snap.stationWaitDuration) + ",";
    json += "\"cooldown\":" + String(snap.irCooldown) + ",";
    json += "\"clamp\":" + String(snap.minSpeedClamp) + ",";
    json += "\"debug\":" + String(enableDebug ? 1 : 0);
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/saveselectpercent", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("val")) {
      targetPercent = request->arg("val").toInt();
    }
    request->send(200, "text/plain", "Percentage Stored");
  });

  server.on("/setled", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("state")) {
      ledState = (request->arg("state").toInt() == 1);
      switchOnboardLED(ledState);
    }
    request->send(200, "text/plain", "LED Updated");
  });

  server.on("/start", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    targetSpeed = map(targetPercent, 0, 100, 0, 220); 
    storedRunSpeed = targetSpeed; 
    request->send(200, "text/plain", "Started");
  });

  server.on("/smoothstop", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    targetSpeed = 0; 
    request->send(200, "text/plain", "Smooth Stop Active");
  });

  server.on("/setdir", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("dir")) {
      bool targetDir = (request->arg("dir") == "forward");
      if (currentSpeed > 0 && isForward != targetDir) {
        targetSpeed = 0;
        while (currentSpeed > 0) {
          processMomentum(millis());
          delay(1);
        }
      }
      isForward = targetDir;
      targetSpeed = storedRunSpeed; 
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/updatephysics", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("step") && request->hasArg("interval") && request->hasArg("wait") && request->hasArg("cooldown") && request->hasArg("clamp")) {
      config.rampStep = request->arg("step").toInt();
      config.rampInterval = request->arg("interval").toInt();
      config.stationWaitDuration = request->arg("wait").toInt();
      config.irCooldown = request->arg("cooldown").toInt();
      config.minSpeedClamp = request->arg("clamp").toInt();
      saveTrainConfigToFlash(); 
    }
    request->send(200, "text/plain", "Saved");
  });

  server.on("/stop", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    targetSpeed = 0;
    storedRunSpeed = 0;
    currentSpeed = 0; 
    currentState = RUNNING; 
    applyTrackPower();
    request->send(200, "text/plain", "Emergency Stop Executed");
  });

  server.on("/setledmode", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Muted");
  });

  server.on("/setdebug", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("state")) {
      enableDebug = (request->arg("state").toInt() == 1);
      enableConnectedTime(enableDebug);
      Serial.printf("[%lu ms] System state parameters shifted: enableDebug = %s\n", millis(), enableDebug ? "TRUE" : "FALSE");
    }
    request->send(200, "text/plain", "Debug Target State Synced");
  });
  
  server.begin();
  isProcessingDisconnect = false; 
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

  server.on("/", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1.0'>";
    html += "<style>body{font-family:Arial;background:#111;color:#fff;text-align:center;padding:20px;}";
    html += "input{display:block;width:80%;max-width:300px;margin:15px auto;padding:12px;background:#222;border:1px solid #444;color:#fff;border-radius:6px;}";
    html += "button{background:#00adb5;color:#fff;border:none;padding:12px 30px;font-size:16px;font-weight:bold;border-radius:6px;cursor:pointer;}</style>";
    html += "</head><body><h2>WiFi Setup Portal</h2><form action='/savewifi' method='GET'>";
    html += "<input type='text' name='s' placeholder='WiFi SSID' required>";
    html += "<input type='password' name='p' placeholder='WiFi Password'>";
    html += "<button type='submit'>SAVE AND CONNECT</button></form></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/savewifi", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
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
  });
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

void WiFiEvent(WiFiEvent_t event) {
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
        Serial.printf("[%lu ms] Asynchronous hardware drop frame. Triggering recovery...\n", now);
        setOLEDLine1("LINK LOST");
        enableConnectedTime(false);
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
      unsigned int hours   = (totalSeconds / 3600) % 24;
      unsigned int days    = totalSeconds / 86400;

      char uptimeBuffer[11];

      if (days == 0) {
        snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%02uh%02um%02us", hours, minutes, seconds);
      } else {
        snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%02ud%02uh%02us", days, hours, seconds);
      }
      
      setOLEDLine2(uptimeBuffer);
    }
  }
}
