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

    // scan attributes
    uint8_t target_id;

public:
    NESPtonBot(/* args */);
    void init();
    void connect();
    void processRecv();
    double simShot(double power, double angle, bool approx);
    void scanFor(bool *success, Vec2d *launch_params);
    bool checkForAbortCondition();
    void targetPlayers();
    void iterate(double *power, Vec2d *power_limits, double *angle, Vec2d *angle_limits, Vec2d *distances, bool *approx, int *i, int *state, bool mode);
};

#endif