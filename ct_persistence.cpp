#include <Arduino.h>
#include <Preferences.h>
#include "ct_persistence.h"

volatile WifiConfig wificonfig;

void saveWifiConfigToFlash() {
  Preferences prefs;
  prefs.begin("wifi-core", false);
  prefs.putBytes("netConfig", (const void*)&wificonfig, sizeof(WifiConfig));
  prefs.end();
  Serial.printf("[%lu ms] WiFi structural credentials stored securely inside wifi-core.\n", millis());
}

void loadWifiConfigfromFlash() {
  Preferences prefs;
  prefs.begin("wifi-core", true);

  if (prefs.isKey("netConfig")) {
    prefs.getBytes("netConfig", (void*)&wificonfig, sizeof(WifiConfig));
    Serial.printf("[%lu ms] WiFi link parameters parsed successfully from flash memory.\n", millis());
  } else {
    Serial.printf("[%lu ms] No link configurations found. Generating system baseline defaults...\n", millis());
    prefs.end();

    memset((void*)&wificonfig.wifiSSID, 0, sizeof(wificonfig.wifiSSID));
    memset((void*)&wificonfig.wifiPASS, 0, sizeof(wificonfig.wifiPASS));
    strncpy((char*)&wificonfig.wifiSSID, "Canopus", sizeof(wificonfig.wifiSSID) - 1);
    strncpy((char*)&wificonfig.wifiPASS, "YOUR_WIFI_PASSWORD", sizeof(wificonfig.wifiPASS) - 1);

    saveWifiConfigToFlash();
    return;
  }
  prefs.end();
}
