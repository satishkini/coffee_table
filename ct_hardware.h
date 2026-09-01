#ifndef CT_HARDWARE_H
#define CT_HARDWARE_H

#define SPEED_PIN 4   
#define DIR_PIN   5   
#define IR_PIN    3   
#define LED_PIN   8   
#define DISPLAY_BUFFER_SIZE 11
// Dedicated I2C Bus Overrides
#define I2C_SDA_PIN 1
#define I2C_SCL_PIN 2

extern unsigned long trackingTimeLimit;
extern unsigned long connectionCheckInterval;

void initHardware();
void applyTrackPower();
bool readIRSensor(); // Returns true if the sensor beam is broken / track is occupied

void setOLEDLine1(const char* text);
void setOLEDLine2(const char* text);


#endif
