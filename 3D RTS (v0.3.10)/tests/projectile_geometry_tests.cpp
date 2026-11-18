#include "RTSProjectileCollision.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Projectile geometry test requires active Release assertions"
#endif
#include <cmath>
#include <iostream>

static void near(double actual,double expected){
    assert(std::fabs(actual-expected)<1e-8);
}
int main(){
    double t=-1.;
    const Vec3 start(0,1,0),end(10,1,0);
    const Vec3 unit(5,0,0);
    assert(sweptProjectileCylinder(start,end,unit,1.f,2.f,.25f,t));
    near(t,.375); // first boundary, not closest approach at .5
    assert(sweptProjectileCylinder(start,end,Vec3(0,0,0),
                                   1.f,2.f,.25f,t));
    near(t,0.);
    assert(!sweptProjectileCylinder(start,end,Vec3(5,0,3),
                                    1.f,2.f,.25f,t));
    assert(!sweptProjectileCylinder(start,end,Vec3(5,5,0),
                                    1.f,1.f,.25f,t));

    // Vertical-only trajectory and moving from the top down; the XZ
    // circle must not divide by zero or skip the initial Y slab entry.
    assert(sweptProjectileCylinder(Vec3(0,5,0),Vec3(0,0,0),
                                   Vec3(0,0,0),1.f,1.f,0.f,t));
    near(t,.8);
    assert(!sweptProjectileCylinder(Vec3(0,5,0),Vec3(0,2,0),
                                    Vec3(0,0,0),1.f,1.f,0.f,t));
    // Flight over a shorter target must not collide while still above
    // its roof; a lower path overlapping in XZ and Y must collide.
    assert(!sweptProjectileCylinder(Vec3(0,5,0),Vec3(10,0,0),
                                    Vec3(5,0,0),1.f,1.f,0.f,t));
    assert(sweptProjectileCylinder(Vec3(0,5,0),Vec3(10,-5,0),
                                   Vec3(5,0,0),1.f,1.f,0.f,t));
    near(t,.4);

    // Two overlapping hit volumes: the farther Unit center with the
    // larger radius can be hit *first*. Nearest-center selection fails.
    double large=0.,small=0.;
    assert(sweptProjectileCylinder(start,end,Vec3(5,0,0),
                                   2.f,2.f,0.f,large));
    assert(sweptProjectileCylinder(start,end,Vec3(4,0,0),
                                   .25f,2.f,0.f,small));
    near(large,.3);
    near(small,.375);
    assert(large<small);
    // Tangency and zero-length segment both have well-defined inclusive
    // collision parameters; explicit misses do not mutate the result.
    assert(sweptProjectileCylinder(start,end,Vec3(5,0,1),
                                   1.f,2.f,0.f,t));
    near(t,.5);
    t=-123.;
    assert(!sweptProjectileCylinder(Vec3(0,10,0),Vec3(0,10,0),
                                    unit,1.f,2.f,0.f,t));
    near(t,-123.);
    std::cout<<"Projectile geometry: exact first contact, height, tangent, radius, vertical and degenerate segments passed\n";
}
