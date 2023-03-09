#ifndef UTILS_H
#define UTILS_H

#include "bot_config.h"
#include <ESP8266WiFi.h>

int getIgnoredIndex(int arr[], int id);
bool isIgnored(int arr[], int id);
void addToIgnored(int arr[], int id);
void removeFromIgnored(int arr[], int id);

void recvInt(WiFiClient *client, int *target);
void discardBytes(WiFiClient *client, unsigned int byte_count);

#endif