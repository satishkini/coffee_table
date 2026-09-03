#ifndef CT_HARDWARE_H
#define CT_HARDWARE_H

#define SPEED_PIN 4   
#define DIR_PIN   5   
#define IR_PIN    3   

#define I2C_SDA_PIN 8  
#define I2C_SCL_PIN 9  

#define UI_LIGHT_PIN 6 

inline bool readIRSensor() { return (digitalRead(IR_PIN) == LOW); }

inline void switchOnboardLED(bool ledOn){ digitalWrite(UI_LIGHT_PIN, ledOn ? LOW : HIGH); }

void initHardware();
void applyTrackPower();

void setOLEDLine1(const char* text);
//0 = Solid, 1 = Blinking, 2 = Reverse Light Block
void setOLEDLine2(const char* text, uint8_t behavior = 0);

void processDisplayUpdate(unsigned long currentTime);  

#endif