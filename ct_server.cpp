/**
 * @file ct_server.cpp
 * @brief Asynchronous Web Server REST API Endpoints and Watchdog Handlers.
 * 
 * Instantiates the server, definitions, captive portal traps, and endpoint handlers.
 * Network variables are isolated locally here, ensuring raw user session telemetry 
 * never clutters your locomotive physics code.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include "ct_server.h"
#include "ct_hardware.h"
#include "ct_persistence.h"
#include "ct_automation.h"
#include "ct_index.h"

// Enforce backend file-scope encapsulation by defining the server object locally
AsyncWebServer server(80); 

// Localized network variables completely isolated to this file scope
int targetPercent = 0;
unsigned long lastConnectionCheckTime = 0;

// Disconnect tracking parameter constraints
unsigned long disconnectWindowStart = 0;
int disconnectCounter               = 0;
const int maxDisconnectAllowed      = 5; 

// Connect back to the operational parameters owned by the running physics modules
extern int targetSpeed;
extern int storedRunSpeed;
extern int currentSpeed;
extern bool isForward;
extern TrainState currentState;

extern unsigned long trackingTimeLimit;
extern unsigned long connectionCheckInterval;
extern volatile bool isProcessingDisconnect;
extern volatile TrainConfig config;
extern volatile bool ledState;

/**
 * @brief Initializes captive portals, reads connections, and establishes Async routing pipes.
 */
void initServer() {
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA); 
  WiFi.setHostname("Coffee-Table"); 

  // Provision credentials safely via on-demand captive runtime configuration hotspot
  WiFiManager wm;
  if (!wm.autoConnect("Coffee-Table-Train")) {
    setOLEDLine1("NET FAIL");
    
    // CALL YOUR CLEAN HARDWARE RESET HERE
    softwareReset(); // Guarantees a physical master hardware reboot to retry fresh!
  }

  Serial.printf("[%lu ms] Network ready. IP Address: ", millis());
  Serial.println(WiFi.localIP());

  setOLEDLine1("ONLINE");

  // Dashboard landing payload streaming endpoint (RAM-safe flash streaming)
  server.on("/", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send_P(200, "text/html", HTML_PAGE); 
  });

  // Structural Handshake API response target endpoint
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
      ledState = (request->arg("state").toInt() == 1);
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
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
      
      // Enforce controlled braking deceleration ramp before throwing physical H-Bridge rails
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
    applyTrackPower(); // Immediate output drop bypasses acceleration profiles for safety
    request->send(200, "text/plain", "Emergency Stop Executed");
  });

  server.begin();
}

/**
 * @brief Evaluates connectivity and intercepts frozen background auto-reconnections.
 */
void processConnectionCheck(unsigned long currentTime) {
  if (currentTime - lastConnectionCheckTime >= connectionCheckInterval) {
    lastConnectionCheckTime = currentTime;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("[%lu ms] Network connection broken! Triggering automated reconnect...\n", currentTime);
      setOLEDLine1("DISCONN");
      
      // FIX: Force handleNetworkDisconnections to execute even if the flag was true,
      // allowing our 1-minute loop to continuously issue new retry attempts!
      handleNetworkDisconnections(currentTime);
    } else {
      // If we are connected, ensure the safety interlock flag is lowered
      isProcessingDisconnect = false;
      disconnectCounter = 0;
    }
  }
}


/**
 * @brief Commands radio driver to re-associate. Forces safety reboot if instability limit hits.
 */
void handleNetworkDisconnections(unsigned long currentTime) {
  isProcessingDisconnect = true;
  WiFi.reconnect(); // Modern Core 3.x background reconnect handler

  if (disconnectCounter == 0) {
    disconnectWindowStart = currentTime;
  }
  disconnectCounter++;
  
  if (currentTime - disconnectWindowStart > trackingTimeLimit) {
    disconnectCounter = 1;
    disconnectWindowStart = currentTime;
  }
  
if (disconnectCounter >= maxDisconnectAllowed) {
  
    Serial.printf("[%lu ms] SERVER WATCHDOG PANIC: Network unstable (%d drops). Resetting...\n", millis(), disconnectCounter);
    setOLEDLine1("NET WDT");
    // CALL YOUR NEW RECOVERY RESET HERE
    softwareReset(); // Guarantees a clean, un-stalleable master hardware reboot!
  }
}

/**
 * @brief Intercepts hardware-level asynchronous WiFi state frames safely.
 */
void WiFiEvent(WiFiEvent_t event) {
  unsigned long now = millis();
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[%lu ms] Obtained stable IP: ", now);
      Serial.println(WiFi.localIP());
      isProcessingDisconnect = false;
      setOLEDLine1("ONLINE");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (!isProcessingDisconnect) {
        Serial.printf("[%lu ms] Asynchronous hardware drop frame. Triggering recovery...\n", now);
        setOLEDLine1("LINK LOST");
        handleNetworkDisconnections(now);
      }
      break;
    default:
      break;
  }
}

/**
 * @brief Abstraction status query helper lets external modules check link health safely.
 */
bool isNetworkLinkStable() {
  return (WiFi.status() == WL_CONNECTED && !isProcessingDisconnect);
}


/**
 * @brief Guaranteed Hardware Watchdog Reset
 * Forces an un-kickable infinite loop that triggers a true hardware silicon reset.
 */
void softwareReset() {
  Serial.println("[Watchdog] Forcing urgent hardware-level master reset...");
  delay(500); // Give the OLED screen a brief window to flush its current text pixels

  // Define a fast 1-second (1000ms) hardware timeout configuration
  esp_task_wdt_config_t reset_config = {
    .timeout_ms = 1000,         // Wait exactly 1 second before firing the reset panic
    .idle_core_mask = (1 << 0), // Target the main core
    .trigger_panic = true       // Enable physical hardware panic reset
  };

  // Forcefully reconfigure the system watchdog to use this fast 1-second timeout
  esp_task_wdt_reconfigure(&reset_config);
  esp_task_wdt_add(NULL); // Subscribe this immediate thread context to the watchdog

  // Disable all software interrupts and enter an un-kickable infinite loop
  portDISABLE_INTERRUPTS();
  while (true) {
    // Stalls right here. Because loop is locked, the hardware timer will panic 
    // and force a true, absolute electrical reset in exactly 1000 milliseconds.
  }
}
