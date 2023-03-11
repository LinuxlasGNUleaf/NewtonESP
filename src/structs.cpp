#include <structs.h>
#include <Arduino.h>

void add(Vec2d *res, Vec2d *a, Vec2d *b)
{
    res->x = a->x + b->x;
    res->y = a->y + b->y;
}

void sub(Vec2d *res, Vec2d *a, Vec2d *b)
{
    res->x = a->x - b->x;
    res->y = a->y - b->y;
}

void mul(Vec2d *res, Vec2d *a, double b)
{
    res->x = a->x * b;
    res->y = a->y * b;
}

double radius_sq(Vec2d *a)
{
    return (a->x) * (a->x) + (a->y) * (a->y);
}

double angle(Vec2d *a)
{
    return atan2(-a->y, a->x);
}