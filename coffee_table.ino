#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>

#include "ct_persistence.h"
#include "ct_automation.h" 
#include "ct_hardware.h"   
#include "ct_server.h"     

volatile TrainConfig config;
TrainState currentState = RUNNING;

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.printf("[%lu ms] System Initialization: Booting clean firmware framework...\n", millis());

  if (!LittleFS.begin(true)) {
    Serial.printf("[%lu ms] CRITICAL: LittleFS Flash Partition Mount Failed!\n", millis());
  } else {
    Serial.printf("[%lu ms] SUCCESS: LittleFS local storage engine mounted cleanly.\n", millis());
  }

  loadTrainConfigFromFlash();
  initHardware();
  initServer();
}

void loop() {
  unsigned long currentTime = millis();

  processConnectionCheck(currentTime); 
  processAutomation(currentTime);      
  processMomentum(currentTime);        
  processOnlineTime(currentTime); 
  
  server.handleClient();
}
