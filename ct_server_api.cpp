#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "ct_common.h" 
#include "ct_train.h"
#include "ct_server.h"
#include "ct_hardware.h"   
#include "ct_persistence.h"
#include "ct_automation.h"

WebServer server(80); 

int targetPercent = 0;
extern bool irTrippedActiveStop;

static bool ledState;
static bool enableDebug = false;

bool isPendingDirectionFlip = false;
bool pendingDirection       = true;

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
  int livePercent = map(train.getCurrentSpeed(), 0, 220, 0, 100);
  if (train.getCurrentSpeed() == 0) livePercent = 0; 

  String json = "{";
  json += "\"ledState\":" + String(ledState ? 1 : 0) + ","; 
  json += "\"dir\":\"" + String(train.isForward() ? "forward" : "reverse") + "\",";
  json += "\"step\":" + String(train.getRampStep()) + ",";
  json += "\"interval\":" + String(train.getRampInterval()) + ",";
  json += "\"wait\":" + String(train.getStationWait()) + ",";
  json += "\"cooldown\":" + String(environmentalIrCooldown) + ",";
  json += "\"clamp\":" + String(train.getMinSpeedClamp()) + ",";
  json += "\"debug\":" + String(enableDebug ? 1 : 0) + ",";
  json += "\"state\":\"" + train.getStateString() + "\",";       
  json += "\"current\":" + String(livePercent) + ",";
  json += "\"target\":" + String(train.getTargetPercent());     
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
  train.setTargetPercent(targetPercent); 
  train.setDefaultSpeed(targetPercent);
  train.saveToFlash(); 
  server.send(200, "text/plain", "Default Speed Saved to Flash");
}

void handleStartTrain() {
  CoffeeTableTrain::State currentState = train.getCurrentState();
  if (currentState == CoffeeTableTrain::EMERGENCY_STOP || 
      currentState == CoffeeTableTrain::STOPPED || 
      currentState == CoffeeTableTrain::AT_STATION) {
    train.setCurrentState(CoffeeTableTrain::RAMPING_UP); 
  }
  train.setTargetPercent(targetPercent); 

  train.setTargetSpeed(map(targetPercent, 0, 100, 0, 220)); 
  train.setStoredRunSpeed(map(targetPercent, 0, 100, 0, 220)); 
  server.send(200, "text/plain", "Started");
}

void handleSmoothStop() {
  CoffeeTableTrain::State currentState = train.getCurrentState();
  if (currentState == CoffeeTableTrain::RUNNING || 
      currentState == CoffeeTableTrain::RAMPING_UP) {
    train.setCurrentState(CoffeeTableTrain::RAMPING_DOWN);
  }
  irTrippedActiveStop = false; 
  train.setTargetSpeed(0); 
  server.send(200, "text/plain", "Smooth Stop Active");
}

void handleSetDirection() {
  if (server.hasArg("dir")) {
    bool targetDir = (server.arg("dir") == "forward");
    
    if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
      train.setCurrentState(CoffeeTableTrain::RUNNING);
    }

    if (train.getCurrentSpeed() > 0 && train.isForward() != targetDir) {
      isPendingDirectionFlip = true;
      pendingDirection = targetDir;
      train.setTargetSpeed(0); 
    } else {
      train.setForward(targetDir);
      train.setTargetSpeed(train.getStoredRunSpeed());
      applyTrackPower();
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleUpdatePhysics() {
  if (server.hasArg("step") && server.hasArg("interval") && server.hasArg("wait") && server.hasArg("cooldown") && server.hasArg("clamp")) {
    train.setRampStep(server.arg("step").toInt());
    train.setRampInterval(server.arg("interval").toInt());
    train.setStationWait(server.arg("wait").toInt());
    environmentalIrCooldown = server.arg("cooldown").toInt();
    train.setMinSpeedClamp(server.arg("clamp").toInt());
    train.saveToFlash(); 
  }
  server.send(200, "text/plain", "Saved");
}

void handleEmergencyStop() {
  train.setTargetSpeed(0);
  train.setStoredRunSpeed(0);
  train.setCurrentSpeed(0); 
  train.setCurrentState(CoffeeTableTrain::EMERGENCY_STOP); 
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
    LOG_PRINTF("System state parameters shifted: enableDebug = %s\n", enableDebug ? "TRUE" : "FALSE");
  }
  server.send(200, "text/plain", "Debug Target State Synced");
}

void handleClearFlash() {
  if (server.hasArg("confirm") && server.arg("confirm") == "true") {
    LOG_PRINTF("FACTORY RESET INITIALIZED: Wiping local configuration blocks...\n");
    enableConnectedTime(false);
    setOLEDLine1("FACTORY");
    setOLEDLine2("RESET   ");
    
    Preferences prefs;
    prefs.begin("train-core", false);
    prefs.clear(); 
    prefs.end();

    prefs.begin("wifi-core", false);
    prefs.clear();
    prefs.end();
    
    LOG_PRINTF("NVRAM wiped cleanly. Executing immediate software restart...\n");
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

bool isDebugEnabled() {return enableDebug ;}
