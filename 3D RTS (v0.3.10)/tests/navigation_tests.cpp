#include "NavigationGrid.hpp"
#include "Math3D.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
static void basic() {
    NavigationGrid g(15,15,2.0f,-15.0f,-15.0f);
    auto path=g.findPath({-10,0,-10},{10,0,10});
    assert(!path.empty());
    assert(path.back().x>8 && path.back().z>8);
    Vec3 p=g.clampWorld({999,12,-999});
    assert(p.y==0 && p.x<=14 && p.z>=-14);
}
static void obstacles() {
    NavigationGrid g(11,11,1.0f,0,0);
    for(int z=0;z<10;++z)g.setBlocked(5,z,true);
    auto path=g.findPath({1.5f,0,1.5f},{9.5f,0,1.5f});
    assert(!path.empty());
    bool usedGap=false;
    for(const auto& p:path) {
       auto c=g.toCell(p);
       assert(g.walkable(c.x,c.z));
       if(c.z==10)usedGap=true;
    }
    assert(usedGap);
    g.setBlocked(5,10,true);
    assert(g.findPath({1.5f,0,1.5f},{9.5f,0,1.5f}).empty());
}
static void noCornerCut() {
    NavigationGrid g(3,3,1.0f,0,0);
    g.setBlocked(1,0,true);g.setBlocked(0,1,true);
    assert(g.findPath({.5f,0,.5f},{2.5f,0,2.5f}).empty());
}
static void weightedDetour(){
    NavigationGrid g(7,3,1.f,0,0);
    for(int x=1;x<=5;++x)g.setTraversalCost(x,1,15.f);
    auto route=g.findPath({.5f,0,1.5f},{6.5f,0,1.5f});
    assert(!route.empty());
    bool detoured=false;
    for(const auto& p:route)if(g.toCell(p).z!=1)detoured=true;
    assert(detoured);
    g.setTraversalCost(1,1,-4.f); // invalid costs are ignored
    assert(g.traversalCost(1,1)==15.f);
}
static void topologyRevisionAndCorridor(){
    NavigationGrid grid(11,11,1.f,0.f,0.f);
    const Vec3 start{1.5f,0,4.5f},goal{9.5f,0,4.5f};
    auto direct=grid.findPath(start,goal);
    assert(direct.size()>=8);
    assert(grid.routeClear(start,direct));
    auto initial=grid.revision();
    grid.setBlocked(5,4,false);
    assert(grid.revision()==initial); // no-op must not cause churn
    grid.setBlocked(5,4,true);
    assert(grid.revision()==initial+1);
    assert(!grid.routeClear(start,direct));
    auto diverted=grid.findPath(start,goal);
    assert(!diverted.empty() && grid.routeClear(start,diverted));
    grid.setBlocked(5,4,true);
    assert(grid.revision()==initial+1);
    grid.setTraversalCost(3,3,5.f);
    assert(grid.revision()==initial+2);
    grid.setTraversalCost(3,3,5.f);
    assert(grid.revision()==initial+2);
    // Corner-cutting becomes invalid even if the next diagonal cell is clear.
    NavigationGrid corner(3,3,1.f,0.f,0.f);
    const Vec3 p{.5f,0,.5f};
    auto diagonal=corner.findPath(p,{2.5f,0,2.5f});
    assert(corner.routeClear(p,diagonal));
    corner.setBlocked(1,0,true);
    assert(!corner.routeClear(p,diagonal));
    assert(corner.routeClear(p,diagonal,diagonal.size()));
}
static void boundedFootprints(){
    // Compare the optimized implementation to the exact brute-force
    // reference, including off-map circles and noninteger grid origins.
    struct Disc { Vec3 center;float radius; };
    const Disc discs[]={
        {{-6.25f,0,12.75f},3.7f},{{47.7f,0,-40.3f},8.3f},
        {{900.f,0,900.f},7.f},{{-55.f,0,-52.f},16.f},
        {{-3.5f,0,8.0f},0.f},{{-15.f,0,2.f},21.f}
    };
    NavigationGrid large(257,193,1.25f,-55.25f,-52.75f);
    for(const auto& disc:discs)
        large.blockDisc(disc.center,disc.radius);
    std::size_t expectedCount=0;
    for(int z=0;z<large.height();++z)for(int x=0;x<large.width();++x){
        const Vec3 center=large.centerOf({x,z});
        bool expected=false;
        for(const auto& disc:discs){
            const double dx=double(center.x)-disc.center.x;
            const double dz=double(center.z)-disc.center.z;
            expected|=(dx*dx+dz*dz<=double(disc.radius)*disc.radius);
        }
        assert(large.walkable(x,z)!=expected);
        expectedCount+=expected?1u:0u;
    }
    assert(large.revision()==expectedCount);
    const auto before=large.revision();
    large.blockDisc({0,0,0},-1.f);
    large.blockDisc({999999,0,999999},2.f);
    large.blockDisc({0,0,0},std::numeric_limits<float>::infinity());
    assert(large.revision()==before);
}
int main() {
    basic();obstacles();noCornerCut();weightedDetour();
    topologyRevisionAndCorridor();
    boundedFootprints();
    std::cout<<"navigation: 4 tests passed\n";
}
