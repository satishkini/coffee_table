#include <Arduino.h>
#include <stdarg.h>
#include "ct_common.h"


RTC_DATA_ATTR int systemRebootReasonCode = REBOOT_CODE_UNKNOWN; 

char circularLogCache[MEMORY_LOG_LIMIT] = {0};

size_t circularLogWriteHead = 0;
volatile bool shouldTriggerReboot = false;
bool systemDebugModeActive = false;

static const unsigned long rebootDelayInterval = 3000;

void appendCircularLog(const char* format, ...) {
    char tempStagingBuffer[256]; // Static local stack frame for safety
    
    // 1. First Pass: Format and inject the standard millisecond timestamp element
    int timestampLength = snprintf(tempStagingBuffer, sizeof(tempStagingBuffer), "[%lu ms] ", millis());
    
    // Safety check: verify the timestamp didn't blow past local stack buffer sizes
    if (timestampLength < 0 || (size_t)timestampLength >= sizeof(tempStagingBuffer)) {
        return; 
    }

    // 2. Second Pass: Safely map standard C variadic arguments straight into the remaining index
    va_list argumentsPointer;
    va_start(argumentsPointer, format);
    vsnprintf(&tempStagingBuffer[timestampLength], sizeof(tempStagingBuffer) - timestampLength, format, argumentsPointer);
    va_end(argumentsPointer);

    size_t lengthOfIncomingString = strlen(tempStagingBuffer);

    // 3. Overflow Protection: If the message risks exceeding the total circular RAM pool bounds
    if (circularLogWriteHead + lengthOfIncomingString >= MEMORY_LOG_LIMIT - 1) {
        clearCircularLog();
        strcpy(circularLogCache, "[Circular Log Reset Due To Cache Overflow]\n");
        circularLogWriteHead = strlen(circularLogCache);
    }

    // 4. Commit Memory Block: Append the text seamlessly into your global logging cache
    strcpy(&circularLogCache[circularLogWriteHead], tempStagingBuffer);
    circularLogWriteHead += lengthOfIncomingString;
}

void clearCircularLog() {
    circularLogWriteHead = 0;
    memset(circularLogCache, 0, MEMORY_LOG_LIMIT);
}

void armDeferredReboot(int reasonCode) {
    if (reasonCode < 1 || reasonCode > 7) {
        systemRebootReasonCode = REBOOT_CODE_UNKNOWN;
    } else {
        systemRebootReasonCode = reasonCode;
    }
    
    shouldTriggerReboot = true;
}

void processRebootTrigger(unsigned long currentTime) {
  static unsigned long rebootTimerStart = 0;
  static bool timerActive = false;

  // Arms cleanly whether tripped by ElegantOTA updates or Factory Clear endpoints
  if (shouldTriggerReboot && !timerActive) {
    rebootTimerStart = currentTime;
    timerActive = true;
    LOG_DEBUG_PRINTF("Deferred Reboot Armed: Awaiting network buffer flush window...\n");
  }

  // Executes a deterministic cold reset after the safety window expires
  if (timerActive && (currentTime - rebootTimerStart >= rebootDelayInterval)) {
    LOG_DEBUG_PRINTF("Executing cold hardware reset sequence via deferred main-loop trigger.\n");
    ESP.restart();
  }
}
