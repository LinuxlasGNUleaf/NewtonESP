#ifndef BOT_CONFIG_H
#define BOT_CONFIG_H

#include <Arduino.h>

// CONNECTION OPTIONS

// primary connection options
const int port = 3490;

// secondary connection options
const unsigned int discard_timeout = 1000;
const unsigned int recv_timeout = 100;
const unsigned int conn_establish_timeout = 6000;
const unsigned int discard_bytes = 178;
const unsigned int wait_timeout = 6000;
const uint8_t version = 9;
inline const char *name = "nESPton";

// SIMULATION OPTIONS
const unsigned int A = 2000000;

const uint8_t max_players = 12;
const uint8_t num_planets = 24;

const double battlefieldW = sqrt(A * 16 / 9); /* 1885 */
const double battlefieldH = sqrt(A * 9 / 16); /* 1060 */
const double player_size = 4.0;
const int margin = 500;

// fixed, do not change
const unsigned int max_segments = 2000;
const unsigned int segment_steps = 25;


// scan settings
const double min_power = 14;
const double max_power = 18;
const double newton_stop_angle = radians(0.01);
const double approx_threshhold = 50;
const double d_angle_divisor = 4;
const int max_iterations = 20;

const double corrector_in_abs_max = 700;
const double correction_threshold = 150;

// fine scan settings
const int fine_steps = 40;

#endif