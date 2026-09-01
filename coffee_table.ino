#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>

#include "ct_persistence.h"
#include "ct_index.h"
#include "ct_automation.h" 
#include "ct_hardware.h"   
#include "ct_server.h"     


volatile TrainConfig config;



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

  processConnectionCheck(currentTime); 
  processAutomation(currentTime);      
  processMomentum(currentTime);        
  processOnlineTime(currentTime); 
 //setOLEDLine2(NULL);
}
