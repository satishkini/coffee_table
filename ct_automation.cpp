#include <Arduino.h>
#include "ct_automation.h"
#include "ct_hardware.h"
#include "ct_persistence.h"

int currentSpeed    = 0;
int targetSpeed     = 0;
int storedRunSpeed  = 0;
bool isForward      = true;

unsigned long lastRampTime      = 0;
unsigned long stationStartTime  = 0;
unsigned long lastIrTriggerTime = 0;
bool irTrippedActiveStop        = false; 

extern volatile TrainConfig config;
extern bool enableDebug;
extern bool isPendingDirectionFlip;
extern bool pendingDirection;

void processAutomation(unsigned long currentTime) {
  if (currentState == EMERGENCY_STOP) {
    return;
  }

  bool irActive = readIRSensor();

  if (irActive && (currentTime - lastIrTriggerTime >= config.irCooldown)) {
    lastIrTriggerTime = currentTime;
    
    if (enableDebug) {
      Serial.printf("[%lu ms] DEBUG: Trackside IR Sensor Triggered!\n", currentTime);
    }

    if (currentState == RUNNING && currentSpeed > 0) {
      currentState = RAMPING_DOWN; 
      targetSpeed = 0;
      irTrippedActiveStop = true; 
    }
  }

  if (currentState == AT_STATION) {
    if (currentTime - stationStartTime >= config.stationWaitDuration) {
      isForward = !isForward;
      targetSpeed = storedRunSpeed;
      currentState = RAMPING_UP; 
    }
  }
}

void processMomentum(unsigned long currentTime) {
  String oledState = "RUN";
  uint8_t alertBehavior = 0; 

  if (currentState == STOPPED) {
    oledState = "STP";
    alertBehavior = 1; // Soft blinking text mode
  } else if (currentState == AT_STATION) {
    oledState = "STA";
    alertBehavior = 1; // Soft blinking text mode
  } else if (currentState == RAMPING_UP) {
    oledState = "RPU";
  } else if (currentState == RAMPING_DOWN) {
    oledState = "RPD";
  } else if (currentState == EMERGENCY_STOP) {
    oledState = "EST";
    alertBehavior = 2; // Solid glowing white block inversion mode
  }

  char oledDir = isForward ? 'F' : 'R';

  int displayPercent = map(currentSpeed, 0, 220, 0, 100);
  if (currentSpeed == 0) displayPercent = 0; 

  char oledLine2Buffer[DISPLAY_BUFFER_SIZE];
  if (currentState != EMERGENCY_STOP) {
    snprintf(oledLine2Buffer, sizeof(oledLine2Buffer), "%s:%c:%03d", 
           oledState.c_str(), oledDir, displayPercent);
    setOLEDLine2(oledLine2Buffer, alertBehavior);
  } else {
    setOLEDLine2("  E-STOP  ", alertBehavior);
  }

  if (currentState == EMERGENCY_STOP) {
    return; 
  }

  if (currentTime - lastRampTime >= config.rampInterval) {
    lastRampTime = currentTime;

    if (currentSpeed != targetSpeed) {
      if (currentSpeed < targetSpeed) {
        if (currentSpeed == 0 && targetSpeed > 0 && targetSpeed > config.minSpeedClamp) {
          currentSpeed = config.minSpeedClamp; 
        } else {
          currentSpeed += config.rampStep;
        }
        if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
      } 
      else {
        if (targetSpeed == 0 && !irTrippedActiveStop && currentState != EMERGENCY_STOP) {
          currentState = RAMPING_DOWN; 
        }
        currentSpeed -= config.rampStep;
        if (targetSpeed == 0 && currentSpeed <= config.minSpeedClamp) {
          currentSpeed = 0; 
        }
        if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
      }

      applyTrackPower();

      if (currentSpeed == 0) {
        if (isPendingDirectionFlip) {
          isPendingDirectionFlip = false;
          isForward = pendingDirection;
          targetSpeed = storedRunSpeed; 
          currentState = RAMPING_UP; 
        } 
        else if (targetSpeed == 0) {
          if (irTrippedActiveStop) {
            currentState = AT_STATION;
            irTrippedActiveStop = false; 
            stationStartTime = millis();
          } else {
            currentState = STOPPED;
            irTrippedActiveStop = false; 
          }
        }
      } else if (currentSpeed == targetSpeed) {
        if (currentSpeed > 0) {
          currentState = RUNNING; 
        }
      }
    }
  }
}
