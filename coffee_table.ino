#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>

#include "ct_common.h"     // NEW: Allocated cross-module variables instantiate cleanly
#include "ct_persistence.h"
#include "ct_train.h" 
#include "ct_automation.h" 
#include "ct_hardware.h"   
#include "ct_server.h"     

CoffeeTableTrain train;

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.printf("[%lu ms] System Initialization: Booting complete object-oriented framework...\n", millis());

  if (!LittleFS.begin(true)) {
    Serial.printf("[%lu ms] CRITICAL: LittleFS Flash Partition Mount Failed!\n", millis());
  } else {
    Serial.printf("[%lu ms] SUCCESS: LittleFS storage engine mounted cleanly.\n", millis());
  }

 

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
