#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>

#include "ct_persistence.h"
#include "ct_index.h"
#include "ct_automation.h" 
#include "ct_hardware.h"   
#include "ct_server.h"     


volatile TrainConfig config;

unsigned long trackingTimeLimit       = 300000;
unsigned long connectionCheckInterval = 60000; 

volatile bool isProcessingDisconnect = false; 


TrainState currentState = RUNNING;

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.printf("[%lu ms] System Initialization: Booting clean firmware framework...\n", millis());

  loadTrainConfigFromFlash();
  initHardware();
  initServer();
}

void loop() {
  unsigned long currentTime = millis();

  processConnectionCheck(currentTime); // Tracks Wi-Fi connectivity states (Server tab)
  processAutomation(currentTime);      // Evaluates station sensor logic (Automation tab)
  processMomentum(currentTime);        // Computes speed adjustments (Automation tab)
  processOnlineTime(currentTime); 
}
