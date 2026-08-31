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
extern volatile bool isProcessingDisconnect;
extern volatile TrainConfig config;

extern bool ledState;

bool isConfigPortalActive = false;

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

    String json = "{";
    json += "\"ledState\":" + String(ledState ? 1 : 0) + ","; 
    json += "\"dir\":\"" + String(isForward ? "forward" : "reverse") + "\",";
    json += "\"step\":" + String(snap.rampStep) + ",";
    json += "\"interval\":" + String(snap.rampInterval) + ",";
    json += "\"wait\":" + String(snap.stationWaitDuration) + ",";
    json += "\"cooldown\":" + String(snap.irCooldown);
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
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
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
    if (request->hasArg("step") && request->hasArg("interval") && request->hasArg("wait") && request->hasArg("cooldown")) {
      config.rampStep = request->arg("step").toInt();
      config.rampInterval = request->arg("interval").toInt();
      config.stationWaitDuration = request->arg("wait").toInt();
      config.irCooldown = request->arg("cooldown").toInt();
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

  server.begin();
  isProcessingDisconnect = false; 
}

void bootConfigPortal() {
  Serial.printf("[%lu ms] Handshake failed. Booting async local setup AP...\n", millis());
  setOLEDLine1("NET FAIL");
  delay(1000);
  
  isConfigPortalActive = true; 
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Coffee-Table-Train"); 
  
  Serial.printf("[%lu ms] Configuration Portal Active. Open IP: ", millis());
  Serial.println(WiFi.softAPIP());
  setOLEDLine1("LOCAL AP");

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
      
      memset((void*)config.wifiSSID, 0, sizeof(config.wifiSSID));
      memset((void*)config.wifiPASS, 0, sizeof(config.wifiPASS));
      strncpy((char*)config.wifiSSID, newSSID.c_str(), sizeof(config.wifiSSID) - 1);
      strncpy((char*)config.wifiPASS, newPASS.c_str(), sizeof(config.wifiPASS) - 1);
      
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
    if (WiFi.status() != WL_CONNECTED && !isProcessingDisconnect && WiFi.getMode() == WIFI_STA) {
      Serial.printf("[%lu ms] Network connection broken! Triggering automated reconnect...\n", currentTime);
      setOLEDLine1("DISCONN");
      handleNetworkDisconnections(currentTime);
    }
  }
}

void handleNetworkDisconnections(unsigned long currentTime) {
  isProcessingDisconnect = true;
  WiFi.reconnect(); 

  if (disconnectCounter == 0) {
    disconnectWindowStart = currentTime;
  }
  disconnectCounter++;
  
  if (currentTime - disconnectWindowStart > trackingTimeLimit) {
    disconnectCounter = 1;
    disconnectWindowStart = currentTime;
  }
  
  if (disconnectCounter >= maxDisconnectAllowed) {
    Serial.printf("[%lu ms] WATCHDOG THRESHOLD REACHED (%d failures). Executing clean emergency software reset...\n", millis(), disconnectCounter);
    setOLEDLine1("NET WDT");
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
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (!isProcessingDisconnect && WiFi.getMode() == WIFI_STA) {
        Serial.printf("[%lu ms] Asynchronous hardware drop frame. Triggering recovery...\n", now);
        setOLEDLine1("LINK LOST");
        handleNetworkDisconnections(now);
      }
      break;
    default:
      break;
  }
}

bool isNetworkLinkStable() {
  return (WiFi.status() == WL_CONNECTED && !isProcessingDisconnect);
}
