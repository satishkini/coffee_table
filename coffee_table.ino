#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManager.h>
#include <Preferences.h> 

// Hardware configuration and pinouts
const int SPEED_PIN = 4;   
const int DIR_PIN   = 5;   
const int IR_PIN    = 3;   
const int RED_LED   = 0;   
const int GREEN_LED = 1;   
const int LED_PIN   = 8;   

// Speed and momentum tracking variables
int targetPercent  = 0;    
int targetSpeed    = 0;    
int storedRunSpeed = 0;    
int currentSpeed   = 0;    
bool isForward     = true; 
bool ledState      = false; 

// Dynamic LED configuration vectors
bool isLedInNetworkMode = false; 
unsigned long lastLedBlinkTime = 0;
const unsigned long ledBlinkInterval = 250; 

volatile unsigned long rampInterval = 15; 
volatile int rampStep               = 2;  

// Automation and state machine settings
enum TrainState { RUNNING, STOPPING, WAITING_AT_STATION };
TrainState currentState = RUNNING;

unsigned long lastRampTime       = 0;
unsigned long stationTimerStart  = 0;
unsigned long lastIRTriggerTime  = 0;

volatile unsigned long stationWaitDuration = 4000; 
volatile unsigned long irCooldown          = 5000; 

// Automated reconnect tracking registers
unsigned long disconnectWindowStart   = 0; 
int disconnectCounter                 = 0;
const int maxDisconnectAllowed        = 5; 
const unsigned long trackingTimeLimit = 300000; 

// Connection Check Timing Registers
unsigned long lastConnectionCheckTime      = 0;
const unsigned long connectionCheckInterval = 60000; 

// Interlocking software mutex flag to prevent race conditions
volatile bool isProcessingDisconnect = false; 

AsyncWebServer server(80); 
Preferences prefs;

// Include project header modules
#include "index.h"
#include "js.h"
#include "hardware.h"
#include "automation.h"
#include "network.h"

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.printf("[%lu ms] Microcontroller booting up...\n", millis());

  pinMode(LED_PIN, OUTPUT);
  loadPhysicsFromFlash();
  
  if(!isLedInNetworkMode) {
    digitalWrite(LED_PIN, ledState ? LOW : HIGH); 
  }

  WiFi.onEvent(WiFiEvent);

  pinMode(SPEED_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT_PULLUP); 
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  applyTrackPower(); 
  updateSignalAspect(true); 

  WiFi.mode(WIFI_STA); 
  WiFi.setHostname("Z-Scale-Throttle"); 

  WiFiManager wm;
  if (!wm.autoConnect("Coffee-Table-Z-Train")) {
    delay(3000);
    ESP.restart();
  }

  Serial.printf("[%lu ms] Network ready. Managed via Pi-hole DNS allocation.\n", millis());
  setupWebServer();
}

void loop() {
  unsigned long currentTime = millis();
  processConnectionCheck(currentTime);
  processAutomation(currentTime);
  processMomentum(currentTime);
  processLedBlinking(currentTime); 
}
