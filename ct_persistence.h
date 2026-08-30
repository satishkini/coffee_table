#ifndef CT_PERSISTENCE_H
#define CT_PERSISTENCE_H

#include <Arduino.h>

struct TrainConfig {
  unsigned long rampInterval;        // Step interval delay (ms)
  int rampStep;                      // Velocity increment size
  unsigned long stationWaitDuration; // Platform dwell duration (ms)
  unsigned long irCooldown;          // Sensor dead-time window (ms)
};

void saveTrainConfigToFlash();
void loadTrainConfigFromFlash();

#endif
