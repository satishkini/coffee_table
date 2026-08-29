#ifndef CT_HARDWARE_H
#define CT_HARDWARE_H

#define SPEED_PIN 4   
#define DIR_PIN   5   
#define IR_PIN    3   
#define LED_PIN   8   

extern unsigned long trackingTimeLimit;
extern unsigned long connectionCheckInterval;

void applyTrackPower();
bool readIRSensor(); // Returns true if the sensor beam is broken / track is occupied
void processLedBlinking(unsigned long currentTime);
void initHardware();

#endif
