#include <Arduino.h>
#include <WiFi.h>
#include "ct_hardware.h"
#include "ct_persistence.h" 
#include "ct_server.h"

extern int currentSpeed;
extern bool isForward;

// Access structural parameters managed by the core/persistence threads
extern volatile TrainConfig config;

unsigned long lastLedBlinkTime   = 0;
const unsigned long ledBlinkInterval = 250; 

volatile bool ledState = false ;

void initHardware() {
  pinMode(LED_PIN, OUTPUT);
  
  loadTrainConfigFromFlash();
  
  if (!config.isLedInNetworkMode) {
    digitalWrite(LED_PIN, ledState ? LOW : HIGH); 
  }

  pinMode(SPEED_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT_PULLUP); 
  
  analogWriteFrequency(SPEED_PIN, 20000); 
  
  applyTrackPower(); 
}


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

bool readIRSensor() {
  return (digitalRead(IR_PIN) == LOW);
}

void processLedBlinking(unsigned long currentTime) {
  if (!config.isLedInNetworkMode) return; 

  if (isNetworkLinkStable()) {
    digitalWrite(LED_PIN, LOW); // Solid blue on stable connection
  } else {
    if (currentTime - lastLedBlinkTime >= ledBlinkInterval) {
      lastLedBlinkTime = currentTime;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
    }
  }
}
