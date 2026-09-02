#ifndef CT_PERSISTENCE_H
#define CT_PERSISTENCE_H

#include <Arduino.h>

struct WifiConfig {
  char wifiSSID[32];                 
  char wifiPASS[64];                 
};

void saveWifiConfigToFlash();
void loadWifiConfigFromFlash();

extern volatile WifiConfig wificonfig;

#endif

