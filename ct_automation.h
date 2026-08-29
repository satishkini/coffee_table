#ifndef CT_AUTOMATION_H
#define CT_AUTOMATION_H

enum TrainState { RUNNING, STOPPING, WAITING_AT_STATION };

void processAutomation(unsigned long currentTime);
void processMomentum(unsigned long currentTime);

#endif
