#ifndef STRUCTS_H
#define STRUCTS_H
#include <math.h>

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

void add(Vec2d *res, Vec2d *a, Vec2d *b);
void sub(Vec2d *res, Vec2d *a, Vec2d *b);
void mul(Vec2d *res, Vec2d *a, double b);
double radius_sq(Vec2d *a);

#endif