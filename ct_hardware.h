#ifndef CT_HARDWARE_H
#define CT_HARDWARE_H

#define SPEED_PIN 4   
#define DIR_PIN   5   
#define IR_PIN    3   
#define LED_PIN   8   

#define I2C_SDA_PIN 1
#define I2C_SCL_PIN 2

#define DISPLAY_BUFFER_SIZE 11

extern unsigned long trackingTimeLimit;
extern unsigned long connectionCheckInterval;

void initHardware();
void applyTrackPower();
bool readIRSensor(); 
void switchOnboardLED(bool ledOn);

void setOLEDLine1(const char* text);
//0 = Solid, 1 = Blinking, 2 = Reverse Light Block
void setOLEDLine2(const char* text, uint8_t behavior = 0);


#endif
