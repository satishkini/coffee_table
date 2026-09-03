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
  unsigned long _forwardStationDelay;
  unsigned long _reverseStationDelay;


public:
  CoffeeTableTrain();

  inline int getCurrentSpeed() const { return _currentSpeed; }
  inline int getTargetSpeed() const { return _targetSpeed; }
  inline int getStoredRunSpeed() const { return _storedRunSpeed; }
  inline int getTargetPercent() const { return _targetPercent; }
  inline bool isForward() const { return _isForward; }
  inline State getCurrentState() const { return _currentState; }

  inline unsigned long getRampInterval() const { return _rampInterval; }
  inline int getRampStep() const { return _rampStep; }
  inline unsigned long getStationWait() const { return _stationWaitDuration; }
  inline int getMinSpeedClamp() const { return _minSpeedClamp; }
  inline int getDefaultSpeed() const { return _defaultSpeed; }
  inline unsigned long getForwardStationDelay() const { return _forwardStationDelay; }
  inline unsigned long getReverseStationDelay() const { return _reverseStationDelay; }

  inline void setCurrentSpeed(int speed) { _currentSpeed = constrain(speed, 0, 220); }
  inline void setTargetSpeed(int speed) { _targetSpeed = constrain(speed, 0, 220); }
  inline void setStoredRunSpeed(int speed) { _storedRunSpeed = constrain(speed, 0, 220); }
  inline void setTargetPercent(int percent) { _targetPercent = constrain(percent, 0, 100); }
  inline void setForward(bool forward) { _isForward = forward; }
  inline void setCurrentState(State state) { _currentState = state; }

  inline void setRampInterval(unsigned long ms) { _rampInterval = constrain(ms, 10, 500); }
  inline void setRampStep(int step) { _rampStep = constrain(step, 1, 50); }
  inline void setStationWait(unsigned long ms) { _stationWaitDuration = constrain(ms, 1000, 30000); }
  inline void setMinSpeedClamp(int clamp) { _minSpeedClamp = constrain(clamp, 0, 100); }
  inline void setDefaultSpeed(int speed) { _defaultSpeed = constrain(speed, 0, 100); }
  inline void setForwardStationDelay(unsigned long ms) { _forwardStationDelay = constrain(ms, 0, 5000); }
  inline void setReverseStationDelay(unsigned long ms) { _reverseStationDelay = constrain(ms, 0, 5000); }                


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
