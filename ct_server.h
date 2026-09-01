#ifndef CT_SERVER_H
#define CT_SERVER_H

#include <Arduino.h>
#include <WiFi.h>

// Networking Interface Prototypes
void initServer(); 
void bootConfigPortal();
void processConnectionCheck(unsigned long currentTime);
void WiFiEvent(WiFiEvent_t event);
void handleNetworkDisconnections(unsigned long currentTime);
bool isNetworkLinkStable();
void processOnlineTime(unsigned long currentTime);
void enableConnectedTime(bool enable);
#endif
