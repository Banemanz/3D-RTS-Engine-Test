#include "UnitSpatialIndex.hpp"
#include "Unit.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main(){
    UnitData d;
    std::vector<Unit> units;
    units.reserve(2000);
    for(int i=0;i<2000;++i){
        // Fixed hash-like distribution, including negative cells.
        const int x=((i*37)%353)-177,z=((i*59)%307)-153;
        units.emplace_back(i+1,"Test",d,Vec3(float(x),0,float(z)));
        units.back().ownerId=i%3;
    }
    units[11].isStructure=true;units[11].radius=6.f;
    UnitSpatialIndex index(8.f);
    index.rebuild(units);
    assert(index.activeCount()==units.size());
    assert(index.maxStructureRadius()==6.f);
    for(int id: {1,7,127,1999,2000}){
        assert(index.byId(units,id)!=nullptr);
        assert(index.byId(units,id)->id==id);
    }
    assert(index.byId(units,-1)==nullptr);
    for(int q=0;q<35;++q) {
        const Vec3 center{float(q*11-170),0,float(q*7-118)};
        const float radius=float(q%9+2)*4.f;
        auto candidates=index.nearby(center,radius);
        assert(std::is_sorted(candidates.begin(),candidates.end()));
        assert(std::adjacent_find(candidates.begin(),candidates.end())==candidates.end());
        std::vector<std::size_t> exact;
        for(auto idx:candidates)
            if((units[idx].position-center).lengthXZ()<=radius)exact.push_back(idx);
        std::vector<std::size_t> brute;
        for(std::size_t i=0;i<units.size();++i)
            if((units[i].position-center).lengthXZ()<=radius)brute.push_back(i);
        assert(exact==brute);
    }

    // Update only the moving unit's bucket, preserving original vector order.
    const std::size_t id=120;
    const Vec3 old=units[id].position,newPosition{2.f,0.f,2.f};
    units[id].position=newPosition;
    index.moved(id,old,newPosition);
    auto nearby=index.nearby(newPosition,1.f);
    assert(std::find(nearby.begin(),nearby.end(),id)!=nearby.end());
    nearby=index.nearby(old,1.f);
    assert(std::find(nearby.begin(),nearby.end(),id)==nearby.end());

    // Steering physics should produce exactly the same velocity as all-pairs
    // scanning for a normal small neighbor neighborhood.
    NavigationAgent3D bruteAgent,indexedAgent;
    bruteAgent.setTargetPosition({4.f,0,4.f});
    indexedAgent=bruteAgent;
    const auto v1=bruteAgent.computeSteeringVelocity(units[0],units,1.f/60.f);
    const auto v2=indexedAgent.computeSteeringVelocity(units[0],units,1.f/60.f,&index);
    assert((v1-v2).lengthXZ()<.00001f);

    // Ownership is authoritative even when visual factions are identical:
    // an ally with another visual faction is not a target.
    std::vector<Unit> armies;
    armies.emplace_back(1,"Player",d,Vec3(0,0,0));
    armies.emplace_back(2,"Same visuals / enemy",d,Vec3(3,0,0));
    armies.emplace_back(3,"Different visuals / ally",d,Vec3(2,0,0));
    armies[1].ownerId=1;
    armies[2].data.faction=Faction::Hybrid;
    UnitSpatialIndex small;
    small.rebuild(armies);
    armies[0].update(1.f/60.f,armies,100.f,&small);
    assert(armies[0].targetUnitId==2);
    std::cout<<"spatial: exact broadphase, stable IDs, incremental updates, steering parity and ownership passed\n";
}
