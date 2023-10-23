#pragma once

#include <math.h>

struct SimplePoint {
    double x = 0, y = 0;

    SimplePoint() {};
    SimplePoint(double x, double y) : x(x), y(y) {};
};