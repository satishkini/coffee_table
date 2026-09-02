#ifndef CT_SERVER_H
#define CT_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

void initServer(); 
void bootConfigPortal(); 
void processConnectionCheck(unsigned long currentTime);
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
void handleNetworkDisconnections(unsigned long currentTime);
bool isNetworkLinkStable();

void enableConnectedTime(bool enable);
void processOnlineTime(unsigned long currentTime);

void handleRootDashboard();
void handleStatusUpdate();
void handleSaveSelectPercent();
void handleSetLed();
void handleSaveDefaultSpeed();
void handleStartTrain();
void handleSmoothStop();
void handleSetDirection();
void handleUpdatePhysics();
void handleEmergencyStop();
void handleSetLedMode();
void handleSetDebug();
void handleClearFlash();

void handlePortalRoot();
void handlePortalSaveWifi();

bool isDebugEnabled();

extern unsigned long lastActiveIPTime;
extern WebServer server;
#endif
