#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ElegantOTA.h>

#include "ct_common.h"
#include "ct_train.h"
#include "ct_server.h"
#include "ct_hardware.h"
#include "ct_persistence.h"
#include "ct_automation.h"

extern WebServer server;
extern int targetPercent;

static const int maxDisconnectAllowed = 5;
static const unsigned long connectionCheckInterval = 60000;
static const unsigned long trackingTimeLimit = 300000;
static const unsigned long rebootDelayInterval = 3000;

static unsigned long lastActiveIPTime = 0;
static volatile bool isProcessingDisconnect = false;
static bool displayConnectedTime = true;

volatile bool shouldTriggerReboot = false;

static bool isPortalInitialized = false;

extern bool irTrippedActiveStop;
extern volatile WifiConfig wificonfig;

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

    memset((void*)wificonfig.wifiSSID, 0, sizeof(wificonfig.wifiSSID));
    memset((void*)wificonfig.wifiPASS, 0, sizeof(wificonfig.wifiPASS));
    strncpy((char*)wificonfig.wifiSSID, newSSID.c_str(), sizeof(wificonfig.wifiSSID) - 1);
    strncpy((char*)wificonfig.wifiPASS, newPASS.c_str(), sizeof(wificonfig.wifiPASS) - 1);

    saveWifiConfigToFlash();

    LOG_PRINTF("New credentials saved to Flash. Executing orderly clean software reset...\n");

    server.send(200, "text/plain", "Credentials Saved. Rebooting train layout...");
    
    shouldTriggerReboot = true; 
    
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void bootConfigPortal() {
  if (isPortalInitialized) return; // Guard against multiple configurations

  LOG_PRINTF("Handshake failed. Initializing non-blocking local setup AP...\n");
  setOLEDLine1("NET FAIL");

  // Configure ESP32-C3 soft access point profile without blocking loops
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Coffee-Table");

  LOG_PRINTF("Configuration Portal Active. Open IP: %s\n", WiFi.softAPIP().toString().c_str());
  setOLEDLine1("LOCAL AP");

  // Register routing hooks to endpoint callback methods
  server.on("/", HTTP_GET, handlePortalRoot);
  server.on("/savewifi", HTTP_GET, handlePortalSaveWifi);
  
  // Note: server.begin() is driven at the end of initServer()
  
  isPortalInitialized = true;
  LOG_PRINTF("[AP Mode] Infrastructure online. Awaiting remote client device pairing...\n");
}

void processConfigPortal(unsigned long currentTime) {
  // Only execute text-flashing mechanics if the local portal is actively serving clients
  if (!isPortalInitialized) return;

  static unsigned long lastToggleTime = 0;
  static bool toggleState = false;

  // Track if a client (phone/laptop) has connected to the softAP hardware layer
  int connectedStations = WiFi.softAPgetStationNum();

  if (connectedStations == 0) {
    // Throttled visual anchor flip-flop running non-blockingly at 1Hz (1000ms)
    if (currentTime - lastToggleTime >= 1000) {
      lastToggleTime = currentTime;
      toggleState = !toggleState;
      setOLEDLine1(toggleState ? "PAIR PHONE" : "LOCAL AP");
    }
  } else {
    // A device has connected to the AP layer; present steady configuration status
    static bool wasStationConnected = false;
    if (!wasStationConnected) {
      LOG_PRINTF("[AP Mode] Remote device paired successfully. Awaiting portal web requests...\n");
      setOLEDLine1("PORTAL ACT");
      wasStationConnected = true;
    }
  }
}

void processConnectionCheck(unsigned long currentTime) {
  static unsigned long lastConnectionCheckTime = 0;

  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return;
  }

  if (currentTime - lastConnectionCheckTime >= connectionCheckInterval) {
    lastConnectionCheckTime = currentTime;

    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() == WIFI_STA) {
      LOG_PRINTF("System still offline. Issuing active 60s retry sweep...\n");

      enableConnectedTime(false);
      setOLEDLine1("DISCONN");

      handleNetworkDisconnections(currentTime);
    } else if (WiFi.status() == WL_CONNECTED) {
      isProcessingDisconnect = false;
    }
  }
}

void handleNetworkDisconnections(unsigned long currentTime) {
  static unsigned long disconnectWindowStart = 0;
  static int disconnectCounter = 0;

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
  snprintf(disconnBuffer, DISPLAY_BUFFER_SIZE, "DISCONN:%02u", disconnectCounter);
  setOLEDLine2(disconnBuffer, 1);

  if (disconnectCounter >= maxDisconnectAllowed) {
    LOG_PRINTF("WATCHDOG THRESHOLD REACHED (%d failures). Executing clean emergency software reset...\n", disconnectCounter);
    setOLEDLine1("NET WDT");
    enableConnectedTime(false);
    shouldTriggerReboot = true; 

  }
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  unsigned long now = millis();
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      LOG_PRINTF("Obtained stable link allocation address via Pi-hole: %s\n", WiFi.localIP().toString().c_str());
      isProcessingDisconnect = false;
      setOLEDLine1("ONLINE");
      enableConnectedTime(true);
      lastActiveIPTime = now;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (!isProcessingDisconnect && WiFi.getMode() == WIFI_STA) {
        uint8_t reasonCode = info.wifi_sta_disconnected.reason;
        LOG_PRINTF("Asynchronous hardware drop frame. Reason Code: %u\n", reasonCode);

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

  if (!isDebugEnabled()) {
    return;
  }

  if (currentTime - lastUptimeRefresh >= 1000) {
    lastUptimeRefresh = currentTime;

    if (displayConnectedTime && lastActiveIPTime > 0 && train.getCurrentSpeed() == 0) {
      unsigned long totalSeconds = (currentTime - lastActiveIPTime) / 1000;

      unsigned int seconds = totalSeconds % 60;
      unsigned int minutes = (totalSeconds / 60) % 60;
      unsigned int hours = totalSeconds / 3600;

      char uptimeBuffer[DISPLAY_BUFFER_SIZE];
      snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%02uh%02um%02us", hours, minutes, seconds);

      setOLEDLine2(uptimeBuffer);
    }
  }
}

void initServer() {
  loadWifiConfigFromFlash();
  isProcessingDisconnect = true;
  targetPercent = train.getDefaultSpeed();

  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);

  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setHostname("Coffee-Table");

  setOLEDLine1("CONNECTING");

  WiFi.begin((const char*)wificonfig.wifiSSID, (const char*)wificonfig.wifiPASS);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    LOG_PRINTF(".\n");
  }

  if (WiFi.status() != WL_CONNECTED) {
    bootConfigPortal();
  } else {
    LOG_PRINTF("Network ready. IP Address: %s\n", WiFi.localIP().toString().c_str());
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
  server.on("/triggerReboot", HTTP_GET, handleManualReboot);

  ElegantOTA.setAutoReboot(false);

  ElegantOTA.onStart([]() {
    LOG_PRINTF("OTA Update Started\n");

    setOLEDLine1("OTA UPDATE");
    setOLEDLine2("Starting..");
  });

  // 3. Hook into the END of the update
  ElegantOTA.onEnd([](bool success) {
    shouldTriggerReboot = true; 
    if (success) {
      LOG_PRINTF("OTA Update Success! Deferred reset scheduled.\n");
      setOLEDLine1("UPDATED");
    } else {
      LOG_PRINTF("OTA Update Failed! Clean fallback scheduled.\n");
      setOLEDLine1("FAILED");
    }
  });

  ElegantOTA.begin(&server);

  server.begin();
  isProcessingDisconnect = false;
}


void processRebootTrigger(unsigned long currentTime) {
  static unsigned long rebootTimerStart = 0;
  static bool timerActive = false;

  // Arms cleanly whether tripped by ElegantOTA updates or Factory Clear endpoints
  if (shouldTriggerReboot && !timerActive) {
    rebootTimerStart = currentTime;
    timerActive = true;
    LOG_PRINTF("Deferred Reboot Armed: Awaiting network buffer flush window...\n");
  }

  // Executes a deterministic cold reset after the safety window expires
  if (timerActive && (currentTime - rebootTimerStart >= rebootDelayInterval)) {
    LOG_PRINTF("Executing cold hardware reset sequence via deferred main-loop trigger.\n");
    ESP.restart();
  }
}
