/**
 * @file coffee_table.ino
 * @brief Master Orchestration Skeleton for the Coffee Table Locomotive Controller.
 * 
 * This file serves as the clean C++ execution entry point. Following advanced
 * encapsulation guidelines, all local implementation variables have been shifted
 * out of this file pool and into their respective tab modules. This skeleton strictly
 * handles global data models, core setup triggers, and cyclic loop scheduling.
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>

// 1. Include the unified configuration data structure definition tab first
#include "ct_persistence.h"
// Include remaining layout modules (Ordering satisfies linking dependencies)
#include "ct_index.h"
#include "ct_automation.h" 
#include "ct_hardware.h"   
#include "ct_server.h"     


// Instantiate the single, global, volatile system configuration object model
// These values serve as running system variables and fallback memory defaults
volatile TrainConfig config = {
  15,    // rampInterval: Delay between speed adjustments (ms)
  2,     // rampStep: Velocity increment/decrement step size
  4000,  // stationWaitDuration: Platform dwell time at station standstill (ms)
  5000,  // irCooldown: Optical occupancy sensor dead-time filter window (ms)
  false  // isLedInNetworkMode: LED pin allocation flag (Browser vs. Network Watchdog)
};

// Global Network Watchdog Timing Thresholds (Shared via ct_server.h/ct_hardware.h)
unsigned long trackingTimeLimit       = 300000; // 5 Minutes tracking window limit
unsigned long connectionCheckInterval = 60000;  // 1 Minute cyclic connection check interval

// Interlocking multi-core software mutex flag to prevent async reconnect races
volatile bool isProcessingDisconnect = false; 

// REMOVED: AsyncWebServer server(80); -> Completely deleted from here.
// It is now fully encapsulated inside the top of ct_server.cpp!


// Instantiate the master tracking variable for the station loop state machine
TrainState currentState = RUNNING;

/**
 * @brief Primary hardware boot and module initialization loop.
 */
void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.printf("[%lu ms] System Initialization: Booting clean firmware framework...\n", millis());

  // Invoke unified hardware routine (Configures 20kHz PWM pins, pulls flash bytes)
  initHardware();

  // Invoke unified network routine (Binds WiFiManager captive portals and HTTP routes)
  initServer();
}

/**
 * @brief Continuous cyclic background scheduling loop.
 * Runs millions of times per second with zero thread locks or raw delays.
 */
void loop() {
  unsigned long currentTime = millis();
  
  processConnectionCheck(currentTime); // Tracks Wi-Fi connectivity states (Server tab)
  processAutomation(currentTime);      // Evaluates station sensor logic (Automation tab)
  processMomentum(currentTime);        // Computes speed adjustments (Automation tab)
  processLedBlinking(currentTime);     // Runs status indicator pulses (Hardware tab)
}
