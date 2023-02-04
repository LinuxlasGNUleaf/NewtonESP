#ifndef STRUCTS_H
#define STRUCTS_H

typedef struct
{
   double x;
   double y;
} Vec2d;

typedef struct
{
   Vec2d position;
   double radius;
   double mass;
} Planet;

typedef struct
{
   bool active;
   Vec2d position;
} Player;
#endif