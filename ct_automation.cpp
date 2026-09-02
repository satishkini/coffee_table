#include <Arduino.h>
#include "ct_common.h"
#include "ct_automation.h"
#include "ct_hardware.h"
#include "ct_persistence.h"
#include "ct_train.h"

static unsigned long lastRampTime = 0;
static unsigned long stationStartTime = 0;
static unsigned long lastIrTriggerTime = 0;
bool irTrippedActiveStop = false;

extern bool isPendingDirectionFlip;
extern bool pendingDirection;

void processAutomation(unsigned long currentTime) {
  if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
    return;
  }

  if (readIRSensor() && (currentTime - lastIrTriggerTime >= environmentalIrCooldown)) {
    lastIrTriggerTime = currentTime;

    LOG_PRINTF("DEBUG: Trackside IR Sensor Triggered!\n");

    if (train.getCurrentState() == CoffeeTableTrain::RUNNING && train.getCurrentSpeed() > 0) {
      train.setCurrentState(CoffeeTableTrain::RAMPING_DOWN);
      train.setTargetSpeed(0);
      irTrippedActiveStop = true;
    }
  }

  if (train.getCurrentState() == CoffeeTableTrain::AT_STATION) {
    if (currentTime - stationStartTime >= train.getStationWait()) {
      train.setForward(!train.isForward());
      train.setTargetSpeed(train.getStoredRunSpeed());
      train.setCurrentState(CoffeeTableTrain::RAMPING_UP);
    }
  }
}

void processMomentum(unsigned long currentTime) {
  displayStatus();

  if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
    return;
  }

  if (currentTime - lastRampTime >= train.getRampInterval()) {
    lastRampTime = currentTime;

    if (train.getCurrentSpeed() != train.getTargetSpeed()) {
      if (train.getCurrentSpeed() < train.getTargetSpeed()) {
        if (train.getCurrentSpeed() == 0 && train.getTargetSpeed() > 0 && train.getTargetSpeed() > train.getMinSpeedClamp()) {
          train.setCurrentSpeed(train.getMinSpeedClamp());
        } else {
          train.setCurrentSpeed(train.getCurrentSpeed() + train.getRampStep());
        }
        if (train.getCurrentSpeed() > train.getTargetSpeed()) {
          train.setCurrentSpeed(train.getTargetSpeed());
        }
      } else {
        int nextSpeed = train.getCurrentSpeed() - train.getRampStep();
        if (train.getTargetSpeed() == 0 && nextSpeed <= train.getMinSpeedClamp()) {
          train.setCurrentSpeed(0);
        } else {
          train.setCurrentSpeed(nextSpeed);
        }
        if (train.getCurrentSpeed() < train.getTargetSpeed()) {
          train.setCurrentSpeed(train.getTargetSpeed());
        }
      }

      applyTrackPower();

      LOG_PRINTF("DEBUG: Momentum Shift. Current: %d -> Target: %d | State: %s\n",
                 train.getCurrentSpeed(), train.getTargetSpeed(), train.getStateShortString().c_str());

      if (train.getCurrentSpeed() == 0) {
        if (isPendingDirectionFlip) {
          isPendingDirectionFlip = false;
          train.setForward(pendingDirection);
          train.setTargetSpeed(train.getStoredRunSpeed());
          train.setCurrentState(CoffeeTableTrain::RAMPING_UP);
        } else if (train.getTargetSpeed() == 0) {
          if (irTrippedActiveStop) {
            train.setCurrentState(CoffeeTableTrain::AT_STATION);
            irTrippedActiveStop = false;
            stationStartTime = millis();
          } else {
            train.setCurrentState(CoffeeTableTrain::STOPPED);
            irTrippedActiveStop = false;
          }
        }
      } else if (train.getCurrentSpeed() == train.getTargetSpeed()) {
        if (train.getCurrentSpeed() > 0) {
          train.setCurrentState(CoffeeTableTrain::RUNNING);
        }
      }
    }
  }
}

void   displayStatus() {
  if(isDebugEnabled()) {
    return;
  }
  String oledState = train.getStateShortString();
  uint8_t alertBehavior = 0;

  if (train.getCurrentState() == CoffeeTableTrain::AT_STATION || train.getCurrentState() == CoffeeTableTrain::STOPPED) {
    alertBehavior = 1;
  } else if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
    alertBehavior = 2;
  }

  char oledDir = train.isForward() ? 'F' : 'R';
  int displayPercent = map(train.getCurrentSpeed(), 0, 220, 0, 100);
  if (train.getCurrentSpeed() == 0) displayPercent = 0;

  if ( train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP ) {
    setOLEDLine2("  E-STOP  ", alertBehavior);
  } else {
      char oledLine2Buffer[DISPLAY_BUFFER_SIZE];
      snprintf(oledLine2Buffer, sizeof(oledLine2Buffer), "%s:%c:%03d",
             oledState.c_str(), oledDir, displayPercent);
    setOLEDLine2(oledLine2Buffer, alertBehavior);
          
  }

  
}
