#pragma once
#include "Math3D.hpp"

// GPU-independent ray primitive, shared by camera and unit hit-testing.
struct Ray {
    Vec3 origin;
    Vec3 direction;
};
