#include "RTSAreaShape.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Area geometry tests require Release assertions"
#endif
#include <iostream>
int main(){
    using S=RTSAreaShape;
    const auto inside=[](S shape,double x,double z,double fx,double fz,
                         double r=4.,double angle=90.,double width=2.){
        return rtsAreaContains(shape,x,z,fx,fz,r,angle,width);
    };
    assert(inside(S::Circle,3.,0.,0.,0.));
    assert(!inside(S::Circle,3.01,3.01,0.,0.));
    assert(inside(S::Cone,2.,0.,5.,0.,4.,60.));
    assert(inside(S::Cone,3.,1.,5.,0.,4.,60.));
    assert(!inside(S::Cone,1.,2.,5.,0.,4.,60.));
    assert(!inside(S::Cone,-1.,0.,5.,0.,4.,60.));
    assert(inside(S::Cone,2.,2.,5.,0.,4.,90.));
    assert(inside(S::Cone,0.,3.,0.,10.,4.,60.));
    assert(!inside(S::Cone,3.,0.,0.,10.,4.,60.));
    assert(inside(S::Line,4.,1.,10.,0.,4.,90.,2.));
    assert(!inside(S::Line,4.001,0.,10.,0.,4.,90.,2.));
    assert(!inside(S::Line,2.,1.01,10.,0.,4.,90.,2.));
    assert(!inside(S::Line,-.01,0.,10.,0.,4.,90.,2.));
    assert(inside(S::Line,0.,3.,0.,5.,4.,90.,2.));
    assert(inside(S::Line,2.,0.,0.,0.));
    assert(!inside(S::Line,0.,2.,0.,0.));
    const auto l=rtsAreaOutline(S::Line,10.,11.,1.,0.,4.,90.,2.);
    assert(l.size()==5 && l.front().x==l.back().x &&
           l.front().z==l.back().z);
    const auto c=rtsAreaOutline(S::Cone,10.,11.,0.,1.,4.,90.,2.);
    assert(c.size()==27 && c.front().x==10. && c.front().z==11.);
    assert(rtsAreaOutline(S::Circle,10.,11.,1.,0.,4.,90.,2.).empty());
    std::cout<<"Circle, cone and line geometry passed\n";
}
