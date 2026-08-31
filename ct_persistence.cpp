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
    Serial.println("[Flash Layer] Structural system parameters successfully loaded.");
  } else {
    Serial.println("[Flash Layer] No saved configurations found. Using active structural defaults.");
    config.rampInterval        = 15;
    config.rampStep            = 2;
    config.stationWaitDuration = 4000;
    config.irCooldown          = 5000;

    memset((void*)config.wifiSSID, 0, sizeof(config.wifiSSID));
    memset((void*)config.wifiPASS, 0, sizeof(config.wifiPASS));
  }
  
  prefs.end();
}
