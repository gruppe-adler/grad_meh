#pragma once

#include <math.h>

struct SimplePoint {
    float_t x = 0, y = 0;

    SimplePoint() {};
    SimplePoint(float_t x, float_t y) : x(x), y(y) {};
};