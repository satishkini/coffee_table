#ifndef CT_COMMON_H
#define CT_COMMON_H

#include <Arduino.h>
#include "ct_server.h"

#define DISPLAY_BUFFER_SIZE 11


// --- NEW ENVIRONMENTAL CONFIGURATIONS ---
// Hosted externally to decouple the layout track parameters from the train physics mechanics
extern unsigned long environmentalIrCooldown; 

#define LOG_PRINTF(format, ...) \
    do { \
        if (isDebugEnabled() ) { \
            Serial.printf("[%lu ms] " format, millis(), ##__VA_ARGS__); \
        } \
    } while (0)

#endif
