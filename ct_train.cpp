#include <Arduino.h>
#include <Preferences.h>
#include "ct_train.h"
#include "ct_common.h" 

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
  
  prefs.putULong("irCool", environmentalIrCooldown); 
  
  prefs.end();
  LOG_PRINTF("Train behavior and environmental signatures updated.\n");
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
    
    environmentalIrCooldown = prefs.getULong("irCool");
    _targetPercent = _defaultSpeed; 
    
    LOG_PRINTF("System profiles successfully parsed from flash memory.\n");
  } else {
    LOG_PRINTF("Fresh silicon found. Committing class constructor defaults to Flash...\n");
    prefs.end();
    
    _targetPercent = _defaultSpeed; 
    environmentalIrCooldown = 5000; 
    saveToFlash(); 
    return;
  }
  prefs.end();
}

