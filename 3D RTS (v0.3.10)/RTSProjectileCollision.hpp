#pragma once
#include "Math3D.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

// A fixed-step swept segment versus a vertical capsule-like hit cylinder:
// XZ circle radius = Unit radius + projectile contact padding, and
// Y slab spans Unit base..top padded by the projectile radius.
// Return the earliest inclusive segment parameter in [0,1] (not closest
// approach). Pure math, independent of OS, rendering, tick clocks and Units.
inline bool sweptProjectileCylinder(const Vec3& start,const Vec3& end,
                                    const Vec3& base,float unitRadius,
                                    float unitHeight,float contactRadius,
                                    double& firstContact){
    const double dx=double(end.x)-start.x;
    const double dz=double(end.z)-start.z;
    const double dy=double(end.y)-start.y;
    const double sx=double(start.x)-base.x;
    const double sz=double(start.z)-base.z;
    const double radius=double(unitRadius)+contactRadius;
    const double a=dx*dx+dz*dz;
    const double b=sx*dx+sz*dz;
    const double c=sx*sx+sz*sz-radius*radius;
    double enter=0.,leave=1.;
    if(a<=1e-12){
        if(c>0.)return false;
    }else{
        const double disc=b*b-a*c;
        if(disc<0.)return false;
        const double root=std::sqrt(disc);
        enter=std::max(enter,(-b-root)/a);
        leave=std::min(leave,(-b+root)/a);
        if(enter>leave)return false;
    }
    const double minY=double(base.y)-contactRadius;
    const double maxY=double(base.y)+unitHeight+contactRadius;
    if(std::fabs(dy)<=1e-12){
        if(start.y<minY || start.y>maxY)return false;
    }else{
        double low=(minY-double(start.y))/dy;
        double high=(maxY-double(start.y))/dy;
        if(low>high)std::swap(low,high);
        enter=std::max(enter,low);
        leave=std::min(leave,high);
        if(enter>leave)return false;
    }
    firstContact=enter;
    return true;
}
