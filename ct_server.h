#ifndef CT_SERVER_H
#define CT_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

void initServer(); 
void bootConfigPortal();
void processConnectionCheck(unsigned long currentTime);
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info); 
void handleNetworkDisconnections(unsigned long currentTime);
bool isNetworkLinkStable();

void enableConnectedTime(bool enable);
void processOnlineTime(unsigned long currentTime);

void handleRootDashboard(AsyncWebServerRequest *request);
void handleStatusUpdate(AsyncWebServerRequest *request);
void handleSaveSelectPercent(AsyncWebServerRequest *request);
void handleSetLed(AsyncWebServerRequest *request);
void handleStartTrain(AsyncWebServerRequest *request);
void handleSmoothStop(AsyncWebServerRequest *request);
void handleSetDirection(AsyncWebServerRequest *request);
void handleUpdatePhysics(AsyncWebServerRequest *request);
void handleEmergencyStop(AsyncWebServerRequest *request);
void handleSetLedMode(AsyncWebServerRequest *request);
void handleSetDebug(AsyncWebServerRequest *request);
void handleClearFlash(AsyncWebServerRequest *request);

void handlePortalRoot(AsyncWebServerRequest *request);
void handlePortalSaveWifi(AsyncWebServerRequest *request);

#endif
