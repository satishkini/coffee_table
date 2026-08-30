#include <Arduino.h>
#include <Wire.h>             
#include <Adafruit_GFX.h>     
#include <Adafruit_SSD1306.h> 
#include "ct_hardware.h"
#include "ct_persistence.h" 
#include "ct_server.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 

extern int currentSpeed;
extern bool isForward;

// Access structural parameters managed by the core/persistence threads
extern volatile TrainConfig config;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- NEW: CENTRAL SOFTWARE TEXT CACHE STORAGE ---
// Holds up to 20 characters per row completely isolated in this file memory scope
static char cacheLine1[11] = "STARTING  ";
static char cacheLine2[11] = "          "; 

volatile bool ledState = false ;

static void flushOLED() {
  display.clearDisplay();      // Wipes the entire screen pixel grid instantly
  display.setTextSize(2);      // Sets high-visibility Row Size 2
  display.setTextColor(SSD1306_WHITE);

  // Render current state of Line 1 cache
  display.setCursor(0, 0);
  display.print(cacheLine1);

  // Render current state of Line 2 cache
  display.setCursor(0, 16);
  display.print(cacheLine2);

  display.display();           // Blast the dual-row combined frame to the glass
}


void initHardware() {
  pinMode(LED_PIN, OUTPUT);
  
  loadTrainConfigFromFlash();
  
  digitalWrite(LED_PIN, ledState ? LOW : HIGH); 

  pinMode(SPEED_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT_PULLUP); 
  
  analogWriteFrequency(SPEED_PIN, 20000); 
  
  applyTrackPower(); 

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    flushOLED(); // Shows "STARTING" on boot until Wi-Fi handshakes finish
    Serial.println("[Hardware Layer] High-visibility size 2 cache engine online.");
  }
}

// PUBLIC SETTER: Modifies Row 1 cache from anywhere, then triggers a clean screen flush
void setOLEDLine1(const char* text) {
  // Safely copy incoming text string into our private Row 1 buffer slot
  strncpy(cacheLine1, text, sizeof(cacheLine1) - 1);
  cacheLine1[sizeof(cacheLine1) - 1] = '\0'; // Guarantee trailing null terminator
  
  flushOLED(); // Redraw the complete, synchronized two-line view
}

// PUBLIC SETTER: Modifies Row 2 cache from anywhere, then triggers a clean screen flush
void setOLEDLine2(const char* text) {
  // Safely copy incoming text string into our private Row 2 buffer slot
  strncpy(cacheLine2, text, sizeof(cacheLine2) - 1);
  cacheLine2[sizeof(cacheLine2) - 1] = '\0'; // Guarantee trailing null terminator
  
  flushOLED(); // Redraw the complete, synchronized two-line view
}

// ... [Keep your entire top section, caches, and public line setters completely identical] ...

/**
 * @brief Computes system runtime values from hardware millisecond counters.
 * Updates the Line 2 text cache block once every 1000ms using a clean DD:HH:MM:SS array structure.
 */
void processOLEDBacklightUptime(unsigned long currentTime) {
  // Local static timestamp tracker confines memory allocation scope inside this loop
  static unsigned long lastUptimeRefresh = 0;

  // Non-blocking gate constraints slice execution loop exactly to 1-second intervals
  if (currentTime - lastUptimeRefresh >= 1000) {
    lastUptimeRefresh = currentTime;

    // Calculate time metrics using standard modulo integer arithmetic
    unsigned long totalSeconds = currentTime / 1000;
    unsigned int seconds = totalSeconds % 60;
    unsigned int minutes = (totalSeconds / 60) % 60;
    unsigned int hours   = (totalSeconds / 3600) % 24;
    unsigned int days    = totalSeconds / 86400;

    // Format metrics into a precise 11-byte array container to guarantee bounds safety
    char uptimeBuffer[11];
    if(days == 0){
      snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%02uh%02um%02us", hours, minutes, seconds);
   } else {
    snprintf(uptimeBuffer, sizeof(uptimeBuffer), "%02ud%02uh%02us", days, hours, seconds);
   }

    // public assignment string copies directly into private cache rows and flushes screen
    // Safely leaves Line 1 undisturbed while your Pi-hole network address handles remain visible!
    strncpy(cacheLine2, uptimeBuffer, sizeof(cacheLine2) - 1);
    cacheLine2[sizeof(cacheLine2) - 1] = '\0';
    
    flushOLED();
  }
}


void applyTrackPower() {
  if (currentSpeed == 0) {
    analogWrite(SPEED_PIN, 0);
    digitalWrite(DIR_PIN, LOW);
    return;
  }
  
  if (isForward) {
    digitalWrite(DIR_PIN, LOW);
    analogWrite(SPEED_PIN, currentSpeed);
  } else {
    digitalWrite(DIR_PIN, HIGH);
    analogWrite(SPEED_PIN, 255 - currentSpeed);
  }
}

bool readIRSensor() {
  return (digitalRead(IR_PIN) == LOW);
}
