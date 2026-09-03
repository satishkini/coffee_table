#ifndef CT_COMMON_H
#define CT_COMMON_H

#include <Arduino.h>
#include "ct_server.h"

#define COFFEE_TABLE_HARDWARE_VERSION "ESP32-C3 Train v1"
#define COFFEE_TABLE_FIRMWARE_VERSION "0.1.0"

#define DISPLAY_BUFFER_SIZE 11

extern unsigned long environmentalIrCooldown; 

#define LOG_PRINTF(format, ...) \
    do { \
        if (isDebugEnabled() ) { \
            Serial.printf("[%lu ms] " format, millis(), ##__VA_ARGS__); \
        } \
    } while (0)

#endif
