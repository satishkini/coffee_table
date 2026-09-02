#include <Arduino.h>
#include <Preferences.h>
#include "ct_common.h"
#include "ct_persistence.h"

volatile WifiConfig wificonfig;

void saveWifiConfigToFlash() {
  Preferences prefs;
  prefs.begin("wifi-core", false);
  prefs.putBytes("netConfig", (const void*)&wificonfig, sizeof(WifiConfig));
  prefs.end();
  LOG_PRINTF("WiFi structural credentials stored securely inside wifi-core.\n");
}

void loadWifiConfigFromFlash() {
  Preferences prefs;
  prefs.begin("wifi-core", true);

  if (prefs.isKey("netConfig")) {
    prefs.getBytes("netConfig", (void*)&wificonfig, sizeof(WifiConfig));
    LOG_PRINTF("WiFi link parameters parsed successfully from flash memory.\n");
  } else {
    LOG_PRINTF("No link configurations found. Generating system baseline defaults...\n");
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
