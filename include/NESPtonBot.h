#ifndef NESPTONBOT_H
#define NESPTONBOT_H

#include <bot_config.h>
#include <credentials.h>
#include <structs.h>

#include <vector>
#include <ESP8266WiFi.h>

class NESPtonBot
{
private:
    WiFiClient client;
    uint8_t act_conn;

    uint8_t id;
    double energy;
    bool update_flag;
    // indices
    uint8_t players_index;
    uint8_t planets_index;

    // arrays
    Player *players;
    Planet *planets;
    int *ignored;
public:
    NESPtonBot(/* args */);
    void init();
    void connect();
    void processRecv();
    double simShot(uint8_t target_id, double angle, double velocity);
};

uint8_t getIgnoredIndex(int arr[], int id);
bool isIgnored(int arr[], int id);
void addToIgnored(int arr[], int id);
void removeFromIgnored(int arr[], int id);

void recvInt(WiFiClient *client, int *target);
void discardBytes(WiFiClient *client, unsigned int byte_count);

#endif