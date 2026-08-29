#include <Arduino.h>
#include "ct_automation.h"
#include "ct_hardware.h"
#include "ct_persistence.h"

int targetSpeed    = 0;    
int storedRunSpeed = 0;    
int currentSpeed   = 0;    
bool isForward     = true; 

extern volatile TrainConfig config;

unsigned long lastRampTime      = 0;
unsigned long stationTimerStart = 0;
unsigned long lastIRTriggerTime = 0;

void processAutomation(unsigned long currentTime) {
  extern TrainState currentState;

  switch (currentState) {
    case RUNNING:
      if (readIRSensor() && storedRunSpeed > 0) {
        if (currentTime - lastIRTriggerTime >= config.irCooldown) {
          lastIRTriggerTime = currentTime;
          targetSpeed = 0;       
          currentState = STOPPING;
        }
      }
      break;

    case STOPPING:
      if (currentSpeed == 0) {
        stationTimerStart = currentTime; 
        currentState = WAITING_AT_STATION;
      }
      break;

    case WAITING_AT_STATION:
      if (currentTime - stationTimerStart >= config.stationWaitDuration) {
        targetSpeed = storedRunSpeed;    
        currentState = RUNNING;          
      }
      break;
  }
}

void processMomentum(unsigned long currentTime) {
  if (currentTime - lastRampTime >= config.rampInterval) {
    lastRampTime = currentTime; 
    
    if (currentSpeed < targetSpeed) {
      currentSpeed = min(currentSpeed + config.rampStep, targetSpeed);
      applyTrackPower();
    } 
    else if (currentSpeed > targetSpeed) {
      currentSpeed = max(currentSpeed - config.rampStep, targetSpeed);
      applyTrackPower();
    }
  }
}
