#ifndef BOT_CONFIG_H
#define BOT_CONFIG_H

#include <Arduino.h>

// primary connection options
const int port = 3490;

// secondary connection options
const unsigned int discard_timeout = 1000;
const unsigned int recv_timeout = 100;
const unsigned int reconnect_wait = 8000;
const uint8_t version = 9;
inline const char* name = "nESPton";

// game options
const uint8_t max_players = 50;
const uint8_t max_planets = 30;

#endif