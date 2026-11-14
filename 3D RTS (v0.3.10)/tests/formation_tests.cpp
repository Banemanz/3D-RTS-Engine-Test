#include "RTSFormation.hpp"
#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main(){
    const Vec3 center{18.f,0.f,26.f};
    std::vector<RTSFormationMember> soldiers;
    for(int z=0;z<3;++z)for(int x=0;x<3;++x)
        soldiers.push_back({1+z*3+x,
            Vec3(float(x*2-30),0.f,float(z*2-10)),.65f});
    const auto arranged=arrangeRTSFormation(soldiers,center);
    assert(arranged.size()==soldiers.size());
    Vec3 mean{};
    for(std::size_t i=0;i<arranged.size();++i){
        assert(arranged[i].id==soldiers[i].id);
        mean+=arranged[i].destination;
        for(std::size_t j=i+1;j<arranged.size();++j)
            assert((arranged[i].destination-arranged[j].destination).lengthXZ()>=1.79f);
    }
    mean=mean*(1.f/float(arranged.size()));
    assert((mean-center).lengthXZ()<.01f);
    std::reverse(soldiers.begin(),soldiers.end());
    const auto shuffled=arrangeRTSFormation(soldiers,center);
    for(const auto& slot:shuffled){
        const auto match=std::find_if(arranged.begin(),arranged.end(),
            [&](const RTSFormationSlot& s){return s.id==slot.id;});
        assert(match!=arranged.end());
        assert((match->destination-slot.destination).lengthXZ()<.0001f);
    }
    const auto wide=arrangeRTSFormation(soldiers,center,5.0f);
    assert(wide.size()==shuffled.size());
    float defaultReach=0.f,wideReach=0.f;
    for(const auto& slot:shuffled)
        defaultReach=std::max(defaultReach,(slot.destination-center).lengthXZ());
    for(const auto& slot:wide)
        wideReach=std::max(wideReach,(slot.destination-center).lengthXZ());
    assert(wideReach>defaultReach*1.5f);
    assert(arrangeRTSFormation({},center).empty());
    const auto single=arrangeRTSFormation({soldiers[0]},center);
    assert(single.size()==1 && single[0].destination.x==center.x &&
           single[0].destination.z==center.z);

    RTSProject custom=RTSProject::defaults();
    // No enemy aggro while validating formation travel, and disable timed
    // reinforcements for this focused movement regression.
    custom.placements.erase(std::remove_if(custom.placements.begin(),
        custom.placements.end(),[](const ProjectPlacement& p) {
            return p.owner==1 && p.catalogId=="rift_raider";
        }),custom.placements.end());
    custom.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    RTSSkirmish game;
    game.loadProject(custom);
    for(auto& unit:game.units)
        unit.isSelected=(unit.ownerId==0 && !unit.isStructure && !unit.isWorker);
    const Vec3 rally{-18.f,0.f,23.f};
    assert(game.orderMove(rally,false));
    for(int tick=0;tick<900;++tick)game.update(1.f/60.f);
    std::vector<Vec3> arrived;
    for(const auto& unit:game.units)
        if(unit.isSelected && unit.isAlive())arrived.push_back(unit.position);
    assert(arrived.size()==4);
    for(const auto& pos:arrived)
        assert((pos-rally).lengthXZ()<6.f);
    for(std::size_t i=0;i<arrived.size();++i)
        for(std::size_t j=i+1;j<arrived.size();++j)
            assert((arrived[i]-arrived[j]).lengthXZ()>.8f);
    std::cout<<"formation: deterministic 9-unit slots, permutation invariance and group travel passed\n";
}
