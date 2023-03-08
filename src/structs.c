#include <structs.h>

void add(Vec2d *res, Vec2d *a, Vec2d *b){
    res->x = a->x = a->x - b->x;
    res->x = a->y - b->y;
}

void sub(Vec2d *res, Vec2d *a, Vec2d *b){
    res->x = a->x - b->x;
    res->y = a->y - b->y;
}

void mul(Vec2d *res, Vec2d *a, double b){
    res->x = a->x * b;
    res->y = a->y * b;
}

void div(Vec2d *res, Vec2d *a, double b){
    res->x = a->x / b;
    res->y = a->y / b;
}

double norm(Vec2d *a){
    return sqrt((a->x)*(a->x) + (a->y)*(a->y));
}