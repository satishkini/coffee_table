#include <Arduino.h>
#include <Preferences.h>
#include "ct_common.h"
#include "ct_persistence.h"

volatile WifiConfig wificonfig;

void saveWifiConfigToFlash() {
  Preferences prefs;
  
  // 1. Create a stable, local stack copy to strip volatile safely
  WifiConfig localConfig;
  
  // 2. Perform a byte-exact block copy into standard memory space
  memcpy(&localConfig, (const void*)&wificonfig, sizeof(WifiConfig));

  // 3. Drive non-volatile storage API calls from the stable local buffer
  prefs.begin("wifi-core", false);
  prefs.putBytes("netConfig", &localConfig, sizeof(WifiConfig));
  prefs.end();
  
  LOG_PRINTF("WiFi structural credentials stored securely inside wifi-core.\n");
}

void loadWifiConfigFromFlash() {
  Preferences prefs;
  WifiConfig localConfig;
  
  prefs.begin("wifi-core", true);

  if (prefs.isKey("netConfig")) {
    // Read directly into our stable local stack frame
    prefs.getBytes("netConfig", &localConfig, sizeof(WifiConfig));
    prefs.end();
    
    // Atomically copy the verified stack memory back to the volatile profile
    memcpy((void*)&wificonfig, &localConfig, sizeof(WifiConfig));
    
    LOG_PRINTF("WiFi link parameters parsed successfully from flash memory.\n");
  } else {
    LOG_PRINTF("No link configurations found. Generating system baseline defaults...\n");
    prefs.end();

    // Perform operations on stable, non-volatile stack memory without type-punning warnings
    memset(&localConfig, 0, sizeof(WifiConfig));
    
    // Explicit array-decay safety copy with clear space for null termination
    strncpy(localConfig.wifiSSID, "Canopus", sizeof(localConfig.wifiSSID) - 1);
    strncpy(localConfig.wifiPASS, "YOUR_WIFI_PASSWORD", sizeof(localConfig.wifiPASS) - 1);
    
    localConfig.wifiSSID[sizeof(localConfig.wifiSSID) - 1] = '\0';
    localConfig.wifiPASS[sizeof(localConfig.wifiPASS) - 1] = '\0';

    // Update the volatile global footprint in one atomic action
    memcpy((void*)&wificonfig, &localConfig, sizeof(WifiConfig));

    // Persist local stack settings directly down to the flash sector
    saveWifiConfigToFlash();
  }
}
