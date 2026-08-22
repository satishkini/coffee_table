#ifndef HARDWARE_H
#define HARDWARE_H

void applyTrackPower();
void updateSignalAspect(bool greenState);
void savePhysicsToFlash();
void loadPhysicsFromFlash();
void handleNetworkDisconnections(unsigned long currentTime);
void processLedBlinking(unsigned long currentTime);

#endif
