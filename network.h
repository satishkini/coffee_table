#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>

void setupWebServer();
void processConnectionCheck(unsigned long currentTime);
void WiFiEvent(WiFiEvent_t event);

#endif
