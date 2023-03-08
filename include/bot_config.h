#ifndef BOT_CONFIG_H
#define BOT_CONFIG_H

#include <Arduino.h>

// primary connection options
const int port = 3490;

// secondary connection options
const unsigned int discard_timeout = 1000;
const unsigned int recv_timeout = 100;
const unsigned int conn_establish_timeout = 5000;
const unsigned int reconnect_wait = 8000;
const uint8_t version = 9;
inline const char* name = "nESPton";

// OPTIONS COPIED FROM SERVER
const unsigned int A = 2000000;

const uint8_t max_players = 12;
const uint8_t num_planets = 24;

const double battlefieldW = sqrt(A * 16 / 9); /* 1885 */
const double battlefieldH = sqrt(A * 9 / 16); /* 1060 */
const double player_size = 4.0;
const int margin = 500;

//fixed
const unsigned int max_segments = 2000;
const unsigned int segment_steps = 25;

#endif