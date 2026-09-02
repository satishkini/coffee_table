#include <Arduino.h>
#include <Preferences.h>
#include "ct_train.h"
#include "ct_common.h" 

// Allocate the active environmental variable memory slot
unsigned long environmentalIrCooldown = 5000; 

CoffeeTableTrain::CoffeeTableTrain(): 
    _currentSpeed(0), _targetSpeed(0), _storedRunSpeed(0), _targetPercent(40), 
    _isForward(true), _currentState(STOPPED),
    _rampInterval(15), _rampStep(2), _stationWaitDuration(4000), 
    _minSpeedClamp(35), _defaultSpeed(40) { 
      loadFromFlash();
}



void CoffeeTableTrain::saveToFlash() {
  Preferences prefs;
  prefs.begin("train-core", false);
  
  prefs.putULong("rampInt", _rampInterval);
  prefs.putInt("rampStep", _rampStep);
  prefs.putULong("statWait", _stationWaitDuration);
  prefs.putInt("speedClamp", _minSpeedClamp);
  prefs.putInt("defSpeed", _defaultSpeed);
  
  // SECURE FLASH PIPELINE: Writes the environmental layout variable to preferences namespaces
  prefs.putULong("irCool", environmentalIrCooldown); 
  
  prefs.end();
  Serial.printf("[%lu ms] Train behavior and environmental signatures updated.\n", millis());
}

void CoffeeTableTrain::loadFromFlash() {
  Preferences prefs;
  prefs.begin("train-core", true);
  
  if (prefs.isKey("rampInt")) {
    _rampInterval        = prefs.getULong("rampInt");
    _rampStep            = prefs.getInt("rampStep");
    _stationWaitDuration = prefs.getULong("statWait");
    _minSpeedClamp       = prefs.getInt("speedClamp");
    _defaultSpeed        = prefs.getInt("defSpeed");
    
    // SECURE FLASH PIPELINE: Reads the environmental layout variable safely from memory blocks
    environmentalIrCooldown = prefs.getULong("irCool");
    _targetPercent = _defaultSpeed; 
    
    Serial.printf("[%lu ms] System profiles successfully parsed from flash memory.\n", millis());
  } else {
    Serial.printf("[%lu ms] Fresh silicon found. Committing class constructor defaults to Flash...\n", millis());
    prefs.end();
    
    _targetPercent = _defaultSpeed; 
    environmentalIrCooldown = 5000; // Reset environmental fallback to 5 seconds
    saveToFlash(); 
    return;
  }
  prefs.end();

}
