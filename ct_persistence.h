#ifndef CT_PERSISTENCE_H
#define CT_PERSISTENCE_H

#include <Arduino.h>

struct TrainConfig {
  unsigned long rampInterval;        
  int rampStep;                      
  unsigned long stationWaitDuration; 
  unsigned long irCooldown;          
  char wifiSSID[33];                 
  char wifiPASS[65];
  int minSpeedClamp; 
  int defaultSpeed;                
};

void saveTrainConfigToFlash();
void loadTrainConfigFromFlash();

#endif
