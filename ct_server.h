/**
 * @file ct_server.h
 * @brief Blueprint header mapping web interface routes and watchdog profiles.
 */

#ifndef CT_SERVER_H
#define CT_SERVER_H

#include <Arduino.h>
#include <WiFi.h>

// Networking Interface Prototypes
void initServer(); 
void processConnectionCheck(unsigned long currentTime);
void WiFiEvent(WiFiEvent_t event);
void handleNetworkDisconnections(unsigned long currentTime);
bool isNetworkLinkStable();

#endif
