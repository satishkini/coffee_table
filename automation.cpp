#include <Arduino.h>
#include "automation.h"
#include "hardware.h"

extern const int IR_PIN;
extern int storedRunSpeed;
extern int targetSpeed;
extern int currentSpeed;
extern int rampStep;
extern unsigned long rampInterval;
extern unsigned long lastIRTriggerTime;
extern unsigned long irCooldown;
extern unsigned long stationTimerStart;
extern unsigned long stationWaitDuration;
extern unsigned long lastRampTime;

enum TrainState { RUNNING, STOPPING, WAITING_AT_STATION };
extern TrainState currentState;

void processAutomation(unsigned long currentTime) {
  switch (currentState) {
    case RUNNING:
      if (digitalRead(IR_PIN) == LOW && storedRunSpeed > 0) {
        if (currentTime - lastIRTriggerTime >= irCooldown) {
          lastIRTriggerTime = currentTime;
          updateSignalAspect(false); 
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
      if (currentTime - stationTimerStart >= stationWaitDuration) {
        updateSignalAspect(true);          
        targetSpeed = storedRunSpeed;    
        currentState = RUNNING;          
      }
      break;
  }
}

void processMomentum(unsigned long currentTime) {
  if (currentTime - lastRampTime >= rampInterval) {
    lastRampTime = currentTime; 
    if (currentSpeed < targetSpeed) {
      currentSpeed = min(currentSpeed + rampStep, targetSpeed);
      applyTrackPower();
    } 
    else if (currentSpeed > targetSpeed) {
      currentSpeed = max(currentSpeed - rampStep, targetSpeed);
      applyTrackPower();
    }
  }
}
