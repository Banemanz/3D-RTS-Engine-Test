#pragma once
#include <cmath>
#include <vector>

// Pure fixed-tick ground area search geometry. No platform, renderer, or
// live entity dependencies; circle preserves the legacy center-only search.
enum class RTSAreaShape { Circle, Cone, Line };
struct RTSAreaPoint { double x=0.,z=0.; };
inline RTSAreaPoint rtsAreaFacing(double x,double z){
    const double length=std::sqrt(x*x+z*z);
    return length>1e-9?RTSAreaPoint{x/length,z/length}:RTSAreaPoint{1.,0.};
}
inline bool rtsAreaContains(RTSAreaShape shape,double dx,double dz,
                            double facingX,double facingZ,
                            double radius,double coneDegrees,double lineWidth){
    const double squared=dx*dx+dz*dz;
    if(shape==RTSAreaShape::Circle)return squared<=radius*radius;
    const auto facing=rtsAreaFacing(facingX,facingZ);
    const double ahead=dx*facing.x+dz*facing.z;
    const double lateral=dz*facing.x-dx*facing.z;
    if(ahead<0.)return false;
    if(shape==RTSAreaShape::Cone){
        if(squared>radius*radius)return false;
        constexpr double pi=3.14159265358979323846;
        return std::fabs(lateral)<=ahead*std::tan(coneDegrees*pi/360.);
    }
    return ahead<=radius && std::fabs(lateral)<=lineWidth*.5;
}
// Renderer-only outline uses exactly the same facing/shape parameters.
inline std::vector<RTSAreaPoint> rtsAreaOutline(
        RTSAreaShape shape,double cx,double cz,double fx,double fz,
        double radius,double coneDegrees,double lineWidth){
    if(shape==RTSAreaShape::Circle)return {};
    const auto f=rtsAreaFacing(fx,fz);
    auto point=[&](double ahead,double lateral){
        return RTSAreaPoint{cx+f.x*ahead-f.z*lateral,
                            cz+f.z*ahead+f.x*lateral};
    };
    if(shape==RTSAreaShape::Line){
        const double half=lineWidth*.5;
        return {point(0.,-half),point(radius,-half),point(radius,half),
                point(0.,half),point(0.,-half)};
    }
    constexpr double pi=3.14159265358979323846;
    constexpr int steps=24;
    std::vector<RTSAreaPoint> outline;
    outline.reserve(steps+3);
    outline.push_back({cx,cz});
    for(int i=0;i<=steps;++i){
        const double angle=(double(i)/steps-.5)*coneDegrees*pi/180.;
        outline.push_back(point(radius*std::cos(angle),
                                radius*std::sin(angle)));
    }
    outline.push_back({cx,cz});
    return outline;
}
