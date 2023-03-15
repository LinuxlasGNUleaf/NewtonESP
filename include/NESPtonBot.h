#ifndef NESPTONBOT_H
#define NESPTONBOT_H

#include <bot_config.h>
#include <credentials.h>
#include <structs.h>
#include <utils.h>

#include <ESP8266WiFi.h>
#include <Esp.h>

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
    double *planet_mults;
    int *ignored;

public:
    NESPtonBot(/* args */);
    void init();
    void connect();
    void processRecv(bool silent);
    double simShot(uint8_t target_id, double power, double angle, bool approx);
    void scanFor(uint8_t target_id, bool *success, Vec2d *launch_params);
    bool checkForRelevantUpdate(uint8_t target_id);
    void targetPlayers();
};

#endif