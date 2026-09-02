#ifndef CT_TRAIN_H
#define CT_TRAIN_H

#include <Arduino.h>

class CoffeeTableTrain {
public:
  enum State {
    RUNNING,
    STOPPED,       
    AT_STATION,
    RAMPING_UP,    
    RAMPING_DOWN,  
    EMERGENCY_STOP 
  };

private:
  int _currentSpeed;
  int _targetSpeed;
  int _storedRunSpeed;
  int _targetPercent;
  bool _isForward;
  State _currentState;

  // TRAIN-SPECIFIC PARAMETERS ONLY (Physics properties intrinsic to the loco)
  unsigned long _rampInterval;        
  int _rampStep;                      
  unsigned long _stationWaitDuration; 
  int _minSpeedClamp;                 
  int _defaultSpeed;                  

public:
  CoffeeTableTrain();

  int getCurrentSpeed() const { return _currentSpeed; }
  int getTargetSpeed() const { return _targetSpeed; }
  int getStoredRunSpeed() const { return _storedRunSpeed; }
  int getTargetPercent() const { return _targetPercent; }
  bool isForward() const { return _isForward; }
  State getCurrentState() const { return _currentState; }

  unsigned long getRampInterval() const { return _rampInterval; }
  int getRampStep() const { return _rampStep; }
  unsigned long getStationWait() const { return _stationWaitDuration; }
  int getMinSpeedClamp() const { return _minSpeedClamp; }
  int getDefaultSpeed() const { return _defaultSpeed; }

  void setCurrentSpeed(int speed) { _currentSpeed = constrain(speed, 0, 220); }
  void setTargetSpeed(int speed) { _targetSpeed = constrain(speed, 0, 220); }
  void setStoredRunSpeed(int speed) { _storedRunSpeed = constrain(speed, 0, 220); }
  void setTargetPercent(int percent) { _targetPercent = constrain(percent, 0, 100); }
  void setForward(bool forward) { _isForward = forward; }
  void setCurrentState(State state) { _currentState = state; }

  void setRampInterval(unsigned long ms) { _rampInterval = constrain(ms, 10, 500); }
  void setRampStep(int step) { _rampStep = constrain(step, 1, 50); }
  void setStationWait(unsigned long ms) { _stationWaitDuration = constrain(ms, 1000, 30000); }
  void setMinSpeedClamp(int clamp) { _minSpeedClamp = constrain(clamp, 0, 100); }
  void setDefaultSpeed(int speed) { _defaultSpeed = constrain(speed, 0, 100); }

  String getStateString() const {
    switch (_currentState) {
      case STOPPED:        return "STOPPED";
      case AT_STATION:     return "AT STATION";
      case RAMPING_UP:     return "RAMPING UP";
      case RAMPING_DOWN:   return "RAMPING DOWN";
      case EMERGENCY_STOP: return "EMERGENCY STOP";
      default:             return "RUNNING";
    }
  }

  String getStateShortString() const {
    switch (_currentState) {
      case STOPPED:        return "STP";
      case AT_STATION:     return "STA";
      case RAMPING_UP:     return "RPU";
      case RAMPING_DOWN:   return "RPD";
      case EMERGENCY_STOP: return "EST";
      default:             return "RUN";
    }
  }

  void saveToFlash();
  void loadFromFlash();
};

extern CoffeeTableTrain train;

#endif
