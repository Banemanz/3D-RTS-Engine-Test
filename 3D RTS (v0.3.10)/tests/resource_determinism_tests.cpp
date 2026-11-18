#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

static RTSProject contentionScenario(){
    RTSProject p=RTSProject::defaults();
    p.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    bool nodeFound=false;
    for(auto& place:p.placements){
        if(place.id=="worker_1")place.position={-10.f,0.f,20.f};
        else if(place.id=="worker_2")place.position={-10.f,0.f,28.f};
        else if(place.id=="mineral_north"){
            place.position={-10.f,0.f,24.f};
            place.amount=10;nodeFound=true;
        }
    }
    assert(nodeFound);
    p.placements.erase(std::remove_if(p.placements.begin(),p.placements.end(),
        [](const ProjectPlacement& item){
        return item.id=="mineral_center" || item.id=="mineral_south" ||
               (item.kind=="Unit" && item.owner==1 && item.catalogId=="rift_raider");
    }),p.placements.end());
    p.validate();
    return p;
}
static void issueGather(RTSSkirmish& game,const Vec3& target,bool reversed){
    std::vector<int> ids;
    for(const auto& u:game.units)if(u.catalogId=="worker" && u.ownerId==0)
        ids.push_back(u.id);
    assert(ids.size()==2);
    if(reversed)std::reverse(ids.begin(),ids.end());
    for(const int chosen:ids){
        for(auto& unit:game.units)unit.isSelected=(unit.id==chosen);
        assert(game.orderGather(target));
    }
}
int main(){
    const RTSProject p=contentionScenario();
    RTSSkirmish forward,reverse;
    forward.loadProject(p);reverse.loadProject(p);
    assert(forward.minerals.size()==1 && reverse.minerals.size()==1);
    const Vec3 node=forward.minerals[0].position;
    issueGather(forward,node,false);
    issueGather(reverse,node,true);
    // Distinct insertion order is intentionally invisible to simulation:
    // workers contend for a resource in stable numeric unit-ID order.
    for(int tick=0;tick<900;++tick){
        forward.update(1.f/60.f);
        reverse.update(1.f/60.f);
        assert(forward.playerMinerals==reverse.playerMinerals);
        assert(forward.minerals[0].remaining==reverse.minerals[0].remaining);
        assert(forward.units.size()==reverse.units.size());
        for(std::size_t j=0;j<forward.units.size();++j){
            const auto& a=forward.units[j],&b=reverse.units[j];
            assert(a.id==b.id);
            assert((a.position-b.position).lengthXZ()<0.0001f);
            assert(a.currentHp==b.currentHp);
            assert(a.state==b.state);
        }
    }
    assert(forward.minerals[0].remaining==0);
    std::cout<<"stable simulation order: opposite worker-command insertion produces identical ticks\n";
}
