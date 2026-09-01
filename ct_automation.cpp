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

extern volatile TrainConfig config;
extern bool enableDebug;

void processAutomation(unsigned long currentTime) {
  bool irActive = readIRSensor();

  if (irActive && (currentTime - lastIrTriggerTime >= config.irCooldown)) {
    lastIrTriggerTime = currentTime;
    
    if (enableDebug) {
      Serial.printf("[%lu ms] DEBUG: Trackside IR Sensor Triggered!\n", currentTime);
    }

    if (currentState == RUNNING && currentSpeed > 0) {
      currentState = STOPPING;
      targetSpeed = 0;
    }
  }

  if (currentState == STATION_WAIT) {
    if (currentTime - stationStartTime >= config.stationWaitDuration) {
      isForward = !isForward;
      targetSpeed = storedRunSpeed;
      currentState = STARTING_RAMP;
    }
  }
}

void processMomentum(unsigned long currentTime) {
  if (currentTime - lastRampTime >= config.rampInterval) {
    lastRampTime = currentTime;

    if (currentSpeed != targetSpeed) {
      if (currentSpeed < targetSpeed) {
        currentSpeed += config.rampStep;
        if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
      } else {
        currentSpeed -= config.rampStep;
        if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
      }

      applyTrackPower();

      if (enableDebug) {
        Serial.printf("[%lu ms] DEBUG: Velocity Ramping. Current: %d -> Target: %d\n", currentTime, currentSpeed, targetSpeed);
      }

      if (currentSpeed == 0 && currentState == STOPPING) {
        currentState = STATION_WAIT;
        stationStartTime = millis();
      } else if (currentSpeed == targetSpeed && currentState == STARTING_RAMP) {
        currentState = RUNNING;
      }
    }
  }
}
