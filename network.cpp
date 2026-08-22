#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "network.h"
#include "hardware.h"
#include "index.h"
#include "js.h"

extern const int LED_PIN;
extern int targetPercent;
extern int targetSpeed;
extern int storedRunSpeed;
extern int currentSpeed;
extern bool isForward;
extern bool ledState;
extern bool isLedInNetworkMode;
extern int rampStep;
extern unsigned long rampInterval;
extern unsigned long stationWaitDuration;
extern unsigned long irCooldown;

extern unsigned long lastConnectionCheckTime;
extern const unsigned long connectionCheckInterval;
extern bool isProcessingDisconnect;
extern int disconnectCounter;

extern AsyncWebServer server;
extern Preferences prefs;

enum TrainState { RUNNING, STOPPING, WAITING_AT_STATION };
extern TrainState currentState;

void setupWebServer() {
  server.on("/", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    String fullyAssembledPage = String(HTML_PAGE) + String(HTML_JS_ENGINE);
    request->send(200, "text/html", fullyAssembledPage);
  });

  server.on("/status", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"ledMode\":" + String(isLedInNetworkMode ? 1 : 0) + ",";
    json += "\"ledState\":" + String(ledState ? 1 : 0) + ",";
    json += "\"dir\":\"" + String(isForward ? "forward" : "reverse") + "\",";
    json += "\"step\":" + String(rampStep) + ",";
    json += "\"interval\":" + String(rampInterval) + ",";
    json += "\"wait\":" + String(stationWaitDuration) + ",";
    json += "\"cooldown\":" + String(irCooldown);
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
    if (!isLedInNetworkMode && request->hasArg("state")) {
      ledState = (request->arg("state").toInt() == 1);
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
      prefs.begin("train-core", false);
      prefs.putBool("ledstate", ledState);
      prefs.end();
    }
    request->send(200, "text/plain", "LED Updated");
  });

  server.on("/setledmode", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("network")) {
      isLedInNetworkMode = (request->arg("network").toInt() == 1);
      
      prefs.begin("train-core", false);
      prefs.putBool("ledMode", isLedInNetworkMode);
      prefs.end();
      
      if(!isLedInNetworkMode) {
        digitalWrite(LED_PIN, ledState ? LOW : HIGH);
      }
    }
    request->send(200, "text/plain", "LED Mode Synchronized");
  });

  server.on("/start", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    targetSpeed = map(targetPercent, 0, 100, 0, 220); 
    storedRunSpeed = targetSpeed; 
    if (targetSpeed > 0 && currentState == RUNNING) {
      updateSignalAspect(true); 
    }
    request->send(200, "text/plain", "Started");
  });

  server.on("/smoothstop", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    targetSpeed = 0; 
    request->send(200, "text/plain", "Smooth Stop Active");
  });

  server.on("/setdir", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("dir")) {
      isForward = (request->arg("dir") == "forward");
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/updatephysics", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("step") && request->hasArg("interval") && request->hasArg("wait") && request->hasArg("cooldown")) {
      int s = request->arg("step").toInt();
      int i = request->arg("interval").toInt();
      long w = request->arg("wait").toInt();
      long c = request->arg("cooldown").toInt();
      
      if (s >= 1 && s <= 50) rampStep = s;
      if (i >= 10 && i <= 500) rampInterval = i;
      if (w >= 1000 && w <= 30000) stationWaitDuration = w;
      if (c >= 1000 && c <= 30000) irCooldown = c;
      
      savePhysicsToFlash();
    }
    request->send(200, "text/plain", "Saved");
  });

  server.on("/stop", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    targetSpeed = 0;
    storedRunSpeed = 0;
    currentSpeed = 0; 
    currentState = RUNNING; 
    applyTrackPower();
    updateSignalAspect(false); 
    request->send(200, "text/plain", "Emergency Stop Executed");
  });

  server.begin();
}

void processConnectionCheck(unsigned long currentTime) {
  if (currentTime - lastConnectionCheckTime >= connectionCheckInterval) {
    lastConnectionCheckTime = currentTime;
    
    if (WiFi.status() != WL_CONNECTED && !isProcessingDisconnect) {
      Serial.printf("[%lu ms] Network connection broken! Triggering automated reconnect...\n", currentTime);
      handleNetworkDisconnections(currentTime);
    }
  }
}

void WiFiEvent(WiFiEvent_t event) {
  unsigned long now = millis();
  Serial.printf("[%lu ms] [WiFi-event] event: %d\n", now, event);

  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[%lu ms] Obtained stable IP: ", now);
      Serial.println(WiFi.localIP());
      disconnectCounter = 0; 
      isProcessingDisconnect = false; 
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[%lu ms] Network connection broken! Triggering automated reconnect...\n", now);
      handleNetworkDisconnections(now);
      break;

    default:
      break;
  }
}
