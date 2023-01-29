#ifndef NESPTONBOT_H
#define NESPTONBOT_H

#include <bot_config.h>
#include <credentials.h>

#include <ESP8266WiFi.h>

class NESPtonBot
{
private:
    WiFiClient client;
    
public:
    NESPtonBot(/* args */);
    void init();
    void connect();
};

#endif