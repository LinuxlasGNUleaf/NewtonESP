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
    byte id;
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
};

byte get_ignored_index(int arr[], int id);
bool is_ignored(int arr[], int id);
void add_to_ignore(int arr[], int id);
void remove_from_ignored(int arr[], int id);

#endif