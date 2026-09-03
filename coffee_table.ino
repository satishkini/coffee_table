#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <ElegantOTA.h> 

#include "ct_common.h"     
#include "ct_persistence.h"
#include "ct_train.h" 
#include "ct_automation.h" 
#include "ct_hardware.h"   
#include "ct_server.h"     

CoffeeTableTrain train;

void setup() {
  Serial.begin(115200);
  delay(3000); 
  
  LOG_TRACE_PRINTF("System Initialization: Booting complete object-oriented framework...\n");

  if (!LittleFS.begin(true)) {
    LOG_TRACE_PRINTF("CRITICAL: LittleFS Flash Partition Mount Failed!\n");
  } else {
    LOG_TRACE_PRINTF("SUCCESS: LittleFS storage engine mounted cleanly.\n");
  }

  initHardware();
  initServer();

  LOG_TRACE_PRINTF("BOOT_TRACE: Active system startup initialized. Reason Code: %d\n", getActiveRebootCode());
  
  clearActiveRebootCode(); 
  
  LOG_TRACE_PRINTF("System Status: Version %s execution loops active.\n" , COFFEE_TABLE_FIRMWARE_VERSION);
}

void loop() {
  unsigned long currentTime = millis();

  processConnectionCheck(currentTime); 
  processConfigPortal(currentTime); 

  processAutomation(currentTime);      
  processMomentum(currentTime);        
  processOnlineTime(currentTime); 
  
  processDisplayUpdate(currentTime);

  server.handleClient();
  ElegantOTA.loop();  

  processRebootTrigger(currentTime);
}
