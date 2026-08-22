#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "hardware.h"

extern const int SPEED_PIN;
extern const int DIR_PIN;
extern const int GREEN_LED;
extern const int RED_LED;
extern const int LED_PIN;

extern int currentSpeed;
extern bool isForward;
extern bool ledState;
extern bool isLedInNetworkMode;

extern unsigned long lastLedBlinkTime;
extern const unsigned long ledBlinkInterval;
extern unsigned long rampInterval;
extern int rampStep;
extern unsigned long stationWaitDuration;
extern unsigned long irCooldown;

extern unsigned long disconnectWindowStart;
extern int disconnectCounter;
extern const int maxDisconnectAllowed;
extern const unsigned long trackingTimeLimit;
extern bool isProcessingDisconnect;
extern Preferences prefs;

void applyTrackPower() {
  if (currentSpeed == 0) {
    analogWrite(SPEED_PIN, 0);
    digitalWrite(DIR_PIN, LOW);
    return;
  }
  
  if (isForward) {
    digitalWrite(DIR_PIN, LOW);
    analogWrite(SPEED_PIN, currentSpeed);
  } else {
    digitalWrite(DIR_PIN, HIGH);
    analogWrite(SPEED_PIN, 255 - currentSpeed);
  }
}

void updateSignalAspect(bool greenState) {
  if (greenState) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  } else {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
}

void savePhysicsToFlash() {
  prefs.begin("train-core", false);
  prefs.putULong("rampInt", rampInterval);
  prefs.putInt("rampStp", rampStep);
  prefs.putULong("stnWait", stationWaitDuration);
  prefs.putULong("irCool", irCooldown);
  prefs.end();
}

void loadPhysicsFromFlash() {
  prefs.begin("train-core", true);
  rampInterval        = prefs.getULong("rampInt", 15);
  rampStep            = prefs.getInt("rampStp", 2);
  stationWaitDuration = prefs.getULong("stnWait", 4000);
  irCooldown          = prefs.getULong("irCool", 5000);
  ledState            = prefs.getBool("ledstate", false);
  isLedInNetworkMode  = prefs.getBool("ledMode", false); 
  prefs.end();
}

void handleNetworkDisconnections(unsigned long currentTime) {
  isProcessingDisconnect = true; 
  WiFi.disconnect();
  WiFi.reconnect();

  if (disconnectCounter == 0) {
    disconnectWindowStart = currentTime;
  }
  disconnectCounter++;
  
  if (currentTime - disconnectWindowStart > trackingTimeLimit) {
    disconnectCounter = 1;
    disconnectWindowStart = currentTime;
  }
  
  if (disconnectCounter >= maxDisconnectAllowed) {
    Serial.printf("[%lu ms] Critical network instability detected. Forcing hardware reset...\n", millis());
    delay(1000);
    ESP.restart();
  }
}

void processLedBlinking(unsigned long currentTime) {
  if (!isLedInNetworkMode) return; 

  if (WiFi.status() == WL_CONNECTED && !isProcessingDisconnect) {
    digitalWrite(LED_PIN, LOW); 
  } else {
    if (currentTime - lastLedBlinkTime >= ledBlinkInterval) {
      lastLedBlinkTime = currentTime;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
    }
  }
}
