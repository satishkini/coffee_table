#ifndef CT_COMMON_H
#define CT_COMMON_H

#include <Arduino.h>

#define REBOOT_CODE_UNKNOWN       1  
#define REBOOT_CODE_OTA_FAILED    7  
#define REBOOT_CODE_OTA_SUCCESS   2  
#define REBOOT_CODE_PORTAL_SAVE   3  
#define REBOOT_CODE_WATCHDOG      4  
#define REBOOT_CODE_FACTORY_WIPE  5  
#define REBOOT_CODE_MANUAL_UI     6  

#define COFFEE_TABLE_HARDWARE_VERSION "ESP32-C3 Train v1"
#define COFFEE_TABLE_FIRMWARE_VERSION "0.1.3"

#define DISPLAY_BUFFER_SIZE 11
#define MEMORY_LOG_LIMIT 2048 

extern  char circularLogCache[MEMORY_LOG_LIMIT] ;
extern size_t circularLogWriteHead ;
extern volatile bool shouldTriggerReboot ;
extern bool systemDebugModeActive ;
extern int systemRebootReasonCode;

extern unsigned long environmentalIrCooldown; 

inline bool isDebugEnabled() { return systemDebugModeActive;}
inline void setDebugEnabled(bool enable) {systemDebugModeActive = enable; }

inline void reboot() { shouldTriggerReboot = true; }
inline bool isRebootTriggered() { return shouldTriggerReboot; }

inline char* getCircularLogBuffer() {return circularLogCache; }

inline size_t getCircularLogIndex() { return circularLogWriteHead; }

void appendCircularLog(const char* format, ...);
void clearCircularLog();

void armDeferredReboot(int reasonCode);

inline int getActiveRebootCode() {
    if (systemRebootReasonCode < 1 || systemRebootReasonCode > 7) {
        systemRebootReasonCode = REBOOT_CODE_UNKNOWN;
    }
    return systemRebootReasonCode;
}

inline void clearActiveRebootCode() { systemRebootReasonCode = REBOOT_CODE_UNKNOWN; }

void processRebootTrigger(unsigned long currentTime);

#undef LOG_DEBUG_PRINTF
#define LOG_DEBUG_PRINTF(format, ...) \
    do { \
        if (isDebugEnabled()) { \
            Serial.printf("[%lu ms] " format, millis(), ##__VA_ARGS__); \
            appendCircularLog(format, ##__VA_ARGS__); \
        } \
    } while (0)

#undef LOG_TRACE_PRINTF
#define LOG_TRACE_PRINTF(format, ...) \
    do { \
        Serial.printf("[%lu ms] " format, millis(), ##__VA_ARGS__); \
        appendCircularLog(format, ##__VA_ARGS__); \
    } while (0)
#endif
