#ifndef CT_AUTOMATION_H
#define CT_AUTOMATION_H

#include <Arduino.h>

enum TrainState {
  RUNNING,
  STOPPED,       
  AT_STATION,
  RAMPING_UP,    
  RAMPING_DOWN,  
  EMERGENCY_STOP 
};

extern TrainState currentState;
extern int currentSpeed;
extern int targetSpeed;
extern int storedRunSpeed;
extern bool isForward;

void processAutomation(unsigned long currentTime);
void processMomentum(unsigned long currentTime);

#endif
