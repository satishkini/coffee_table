#include <Arduino.h>
#include "ct_common.h"
#include "ct_automation.h"
#include "ct_hardware.h"
#include "ct_persistence.h"
#include "ct_train.h"

bool irTrippedActiveStop = false;
static unsigned long stationStartTime = 0;

// Non-blocking positioning state markers for asymmetrical loop entries
static bool isWaitingToBrake = false;
static unsigned long brakeTriggerMarker = 0;
static unsigned long activeDelayWindow = 0;

extern bool isPendingDirectionFlip;
extern bool pendingDirection;

void processAutomation(unsigned long currentTime) {
  static unsigned long lastIrTriggerTime = 0;
  
  if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
    return;
  }

  // 1. Scan the trackside IR sensor with debouncing protection
  if (readIRSensor() && (currentTime - lastIrTriggerTime >= environmentalIrCooldown)) {
    lastIrTriggerTime = currentTime;

    if (train.getCurrentState() == CoffeeTableTrain::RUNNING && train.getCurrentSpeed() > 0 && !isWaitingToBrake) {
      isWaitingToBrake = true;
      brakeTriggerMarker = currentTime;
      
      // Calculate layout entry offset delay based on the active direction context
      activeDelayWindow = train.isForward() ? train.getForwardStationDelay() : train.getReverseStationDelay();
      
      LOG_PRINTF("AUTOMATION: IR sensor tripped. Staging brake positioning window: %lu ms\n", activeDelayWindow);
    }
  }

  // 2. Monitor the positioning delay countdown completely non-blockingly
  if (isWaitingToBrake) {
    if (currentTime - brakeTriggerMarker >= activeDelayWindow) {
      isWaitingToBrake = false; // Release the positioning interlock
      
      // Safety confirmation: ensure the train state hasn't changed during the delay window
      if (train.getCurrentState() == CoffeeTableTrain::RUNNING && train.getCurrentSpeed() > 0) {
        train.setCurrentState(CoffeeTableTrain::RAMPING_DOWN);
        train.setTargetSpeed(0);
        irTrippedActiveStop = true;
        LOG_PRINTF("TRACTION: Positioning complete. Commencing station deceleration pattern.\n");
      }
    }
  }

  // 3. Station Timer Management
  if (train.getCurrentState() == CoffeeTableTrain::AT_STATION) {
    if (currentTime - stationStartTime >= train.getStationWait()) {
      // FIXED: Automatic direction inversion removed to prevent reverse loop derailments!
      // The locomotive maintains its polarization context and continues moving forward along the loop.
      train.setTargetSpeed(train.getStoredRunSpeed());
      train.setCurrentState(CoffeeTableTrain::RAMPING_UP);
      
      LOG_PRINTF("AUTOMATION: Station wait expired. Resuming loop trajectory. Target: %d\n", train.getStoredRunSpeed());
    }
  }
}

void processMomentum(unsigned long currentTime) {
  static unsigned long lastRampTime = 0;

  if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
    displayStatus();
    return;
  }

  // Physics calculation execution window step
  if (currentTime - lastRampTime >= train.getRampInterval()) {
    lastRampTime = currentTime;
    displayStatus();

    if (train.getCurrentSpeed() != train.getTargetSpeed()) {
      
      // CASE 1: ACCELERATION (Smooth Linear Scale)
      if (train.getCurrentSpeed() < train.getTargetSpeed()) {
        if (train.getCurrentSpeed() == 0 && train.getTargetSpeed() > 0) {
          // Hardened Anti-Hum Stiction Jump Filter
          int clamp = train.getMinSpeedClamp();
          train.setCurrentSpeed(clamp > train.getTargetSpeed() ? train.getTargetSpeed() : clamp);
        } else {
          train.setCurrentSpeed(train.getCurrentSpeed() + train.getRampStep());
        }
        
        if (train.getCurrentSpeed() > train.getTargetSpeed()) {
          train.setCurrentSpeed(train.getTargetSpeed());
        }
      } 
      
      // CASE 2: DECELERATION (Asymmetrical Braking for Precision Stopping)
      else {
        // Coreless Safety: Brake 2x faster than acceleration to prevent overshoot
        int brakeStep = train.getRampStep() * 2; 
        int nextSpeed = train.getCurrentSpeed() - brakeStep;
        
        if (train.getTargetSpeed() == 0 && nextSpeed <= train.getMinSpeedClamp()) {
          train.setCurrentSpeed(0); // Cut voltage completely to eliminate low-voltage motor hum
        } else {
          train.setCurrentSpeed(nextSpeed);
        }
        
        if (train.getCurrentSpeed() < train.getTargetSpeed()) {
          train.setCurrentSpeed(train.getTargetSpeed());
        }
      }

      // Write newly computed speed adjustments down to the physical H-Bridge registers
      applyTrackPower();

      LOG_PRINTF("TRACTION: Speed: %d/220 | Target: %d | Profile: %s\n",
                 train.getCurrentSpeed(), train.getTargetSpeed(), train.getStateShortString().c_str());

      // Evaluate State Machine Boundaries at Zero Speeds
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
            stationStartTime = millis(); // Lock-in entry timestamp for station wait logic
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

void displayStatus() {
  static unsigned long lastDisplay = 0;
  unsigned long now = millis();

  // Guard block against unnecessary screen buffer processing updates
  if (isDebugEnabled() || (now - lastDisplay < 200)) {
    return;
  }
  lastDisplay = now;

  String oledState = train.getStateShortString();
  uint8_t alertBehavior = 0;

  if (train.getCurrentState() == CoffeeTableTrain::AT_STATION || train.getCurrentState() == CoffeeTableTrain::STOPPED) {
    alertBehavior = 1; // Character Blink
  } else if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
    alertBehavior = 2; // Inverted Full Color Block Flash
  }

  char oledDir = train.isForward() ? 'F' : 'R';
  int displayPercent = map(train.getCurrentSpeed(), 0, 220, 0, 100);
  if (train.getCurrentSpeed() == 0) displayPercent = 0;

  if (train.getCurrentState() == CoffeeTableTrain::EMERGENCY_STOP) {
    setOLEDLine2("  E-STOP  ", alertBehavior);
  } else {
    char oledLine2Buffer[DISPLAY_BUFFER_SIZE];
    snprintf(oledLine2Buffer, sizeof(oledLine2Buffer), "%s:%c:%03d",
             oledState.c_str(), oledDir, displayPercent);
    setOLEDLine2(oledLine2Buffer, alertBehavior);
  }
}
