#include <Arduino.h>
#include <Wire.h>             
#include <Adafruit_GFX.h>     
#include <Adafruit_SSD1306.h> 
#include "ct_common.h" 
#include "ct_train.h"
#include "ct_hardware.h"
#include "ct_persistence.h" 
#include "ct_server.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static char cacheLine1[DISPLAY_BUFFER_SIZE] = "STARTING  ";
static char cacheLine2[DISPLAY_BUFFER_SIZE] = "          "; 
static uint8_t activeBehavior = 0; 

void initHardware() {
  // Hardened Active-LOW Initialization: Stage HIGH register before opening OUTPUT gate
  digitalWrite(UI_LIGHT_PIN, HIGH);
  pinMode(UI_LIGHT_PIN, OUTPUT);

  pinMode(SPEED_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT_PULLUP); 
  
  analogWriteFrequency(SPEED_PIN, 20000); 
  applyTrackPower(); 

  // Initialize standard hardware Wire mapping across open-drain channels 8 and 9
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.display();
    LOG_DEBUG_PRINTF("Peripheral registers online. I2C Display mapped to pins 8/9. UI Light on pin 6.\n");
  }
}

void setOLEDLine1(const char* text) {
  if (text == NULL) return;
  strncpy(cacheLine1, text, DISPLAY_BUFFER_SIZE - 1);
  cacheLine1[DISPLAY_BUFFER_SIZE - 1] = '\0'; 
}

void setOLEDLine2(const char* text, uint8_t behavior) {
  if (text != NULL) {
    strncpy(cacheLine2, text, DISPLAY_BUFFER_SIZE - 1);
    cacheLine2[DISPLAY_BUFFER_SIZE - 1] = '\0'; 
  }
  activeBehavior = behavior;
}

void processDisplayUpdate(unsigned long currentTime) {
  static unsigned long lastOledFlushTime = 0;
  static unsigned long lastFlashToggleTime = 0;
  static bool toggleState = false;

  if (currentTime - lastOledFlushTime < 200) {
    return;
  }
  lastOledFlushTime = currentTime;

  if (currentTime - lastFlashToggleTime >= 400) {
    lastFlashToggleTime = currentTime;
    toggleState = !toggleState;
  }

  display.clearDisplay();      
  display.setTextSize(2);      

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(cacheLine1);

  display.setCursor(0, 16);
  
  if (activeBehavior == 2) {
    if (toggleState) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
      display.print(cacheLine2);
    } else {
      display.setTextColor(SSD1306_WHITE);
      display.print("          "); 
    }
  } else if (activeBehavior == 1) {
    if (toggleState) {
      display.setTextColor(SSD1306_WHITE);
      display.print(cacheLine2);
    } else {
      display.print("          "); 
    }
  } else {
    display.setTextColor(SSD1306_WHITE);
    display.print(cacheLine2);
  }

  // Flushes layout buffer down I2C. Onboard LED on pin 8 will flicker automatically here.
  display.display();
}

void applyTrackPower() {
  if (train.getCurrentSpeed() == 0) {
    analogWrite(SPEED_PIN, 0);
    digitalWrite(DIR_PIN, LOW);
    return;
  }
  if (train.isForward()) {
    digitalWrite(DIR_PIN, LOW);
    analogWrite(SPEED_PIN, train.getCurrentSpeed());
  } else {
    digitalWrite(DIR_PIN, HIGH);
    analogWrite(SPEED_PIN, 255 - train.getCurrentSpeed());
  }
}


