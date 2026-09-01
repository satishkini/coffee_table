#include <Arduino.h>
#include <Preferences.h> 
#include "ct_persistence.h"

extern volatile TrainConfig config;

void saveTrainConfigToFlash() {
  Preferences prefs; 
  prefs.begin("train-core", false);
  prefs.putBytes("sysConfig", (const void*)&config, sizeof(TrainConfig));
  prefs.end();
  Serial.println("[Flash Layer] System configuration bytes atomically saved.");
}

void loadTrainConfigFromFlash() {
  Preferences prefs; 
  
  prefs.begin("train-core", true);
  
  if (prefs.isKey("sysConfig")) {
    prefs.getBytes("sysConfig", (void*)&config, sizeof(TrainConfig));
    Serial.printf("[%lu ms] Structural system parameters successfully loaded.\n", millis());
  } else {
    Serial.printf("[%lu ms] No configurations found. Initializing safe baseline defaults...\n", millis());
    config.rampInterval        = 15;
    config.rampStep            = 2;
    config.stationWaitDuration = 4000;
    config.irCooldown          = 5000;
    config.minSpeedClamp       = 35; 
    
    memset((void*)config.wifiSSID, 0, sizeof(config.wifiSSID));
    memset((void*)config.wifiPASS, 0, sizeof(config.wifiPASS));

    saveTrainConfigToFlash();
  }
  
  prefs.end();
}
