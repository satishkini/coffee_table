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
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static char cacheLine1[DISPLAY_BUFFER_SIZE] = "STARTING  ";
static char cacheLine2[DISPLAY_BUFFER_SIZE] = "          "; 

extern int currentSpeed;
extern bool isForward;

extern volatile TrainConfig config;

static void flushOLED() {
  display.clearDisplay();      
  display.setTextSize(2);      
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(cacheLine1);

  display.setCursor(0, 16);
  display.print(cacheLine2);

  display.display();
}


void initHardware() {
  pinMode(LED_PIN, OUTPUT);
  
  loadTrainConfigFromFlash();
  
  digitalWrite(LED_PIN, HIGH); 

  pinMode(SPEED_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT_PULLUP); 
  
  analogWriteFrequency(SPEED_PIN, 20000); 
    applyTrackPower(); 

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    flushOLED(); 
    Serial.printf("[%lu ms] Peripheral registers and screen cache online.\n", millis());
  }
}

void setOLEDLine1(const char* text) {
  strncpy(cacheLine1, text, sizeof(cacheLine1) - 1);
  cacheLine1[sizeof(cacheLine1) - 1] = '\0'; 
  flushOLED(); 
}

void setOLEDLine2(const char* text) {
  strncpy(cacheLine2, text, sizeof(cacheLine2) - 1);
  cacheLine2[sizeof(cacheLine2) - 1] = '\0'; 
  flushOLED(); 
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

void switchOnboardLED(bool ledOn){
  digitalWrite(LED_PIN, ledOn ? LOW : HIGH);
}
