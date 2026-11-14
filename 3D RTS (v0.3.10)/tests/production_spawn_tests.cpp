#include "RTSSkirmish.hpp"
#include <cassert>
#include <iostream>
int main(){
    RTSSkirmish game;
    const int baseId=game.playerBaseId();
    Unit* base=nullptr;
    for(auto& u:game.units)if(u.id==baseId){base=&u;u.isSelected=true;}
    assert(base);
    const Vec3 basePosition=base->position;
    const auto baseCell=game.navigation.toCell(basePosition);
    assert(!game.navigation.walkable(baseCell.x,baseCell.z));
    assert(game.setRallyPoint({-10,0,-10}));
    assert(game.queueWorker());
    const std::size_t before=game.units.size();
    for(int i=0;i<300;++i)game.update(1.f/60.f);
    assert(game.units.size()>before);
    bool produced=false;
    for(const auto& unit:game.units)if(unit.isWorker && unit.id>int(before)) {
        // Produced units must occupy a legal navigation cell, not the
        // structure footprint or arbitrary blocked tile.
        const auto cell=game.navigation.toCell(unit.position);
        assert(game.navigation.walkable(cell.x,cell.z));
        assert((unit.position-basePosition).lengthXZ()>2.5f);
        produced=true;
    }
    assert(produced);
    for(auto& u:game.units)if(u.id==baseId)u.takeDamage(5000.f,"test");
    game.update(1.f/60.f);
    assert(game.navigation.walkable(baseCell.x,baseCell.z));

    // Harvesting a node completely must release its static pathing footprint.
    RTSSkirmish gathering;
    gathering.minerals[0].remaining=10;
    const Vec3 deposit=gathering.minerals[0].position;
    const auto mineralCell=gathering.navigation.toCell(deposit);
    assert(!gathering.navigation.walkable(mineralCell.x,mineralCell.z));
    bool selected=false;
    for(auto& u:gathering.units)if(u.isWorker && u.ownerId==0 && !selected){
        u.isSelected=true;
        selected=true;
    }
    assert(gathering.orderGather(deposit));
    for(int i=0;i<1200 && gathering.minerals[0].remaining>0;++i)
        gathering.update(1.f/60.f);
    assert(gathering.minerals[0].remaining==0);
    assert(gathering.navigation.walkable(mineralCell.x,mineralCell.z));
    // Prevent new buildings from appearing directly on top of mobile
    // units; rejected construction must not spend any minerals.
    RTSProject safeBuild=RTSProject::defaults();
    for(auto& placement:safeBuild.placements)
        if(placement.id=="worker_1")
            placement.position={-12.f,0.f,24.f};
    safeBuild.validate();
    RTSSkirmish building;
    building.loadProject(safeBuild);
    for(auto& u:building.units)u.isSelected=(u.catalogId=="worker" &&
        (u.position-Vec3{-12.f,0.f,24.f}).lengthXZ()<.2f);
    const int oldResources=building.playerMinerals;
    const auto oldRevision=building.navigation.revision();
    assert(!building.buildBarracks({-12.f,0.f,24.f}));
    assert(building.playerMinerals==oldResources);
    assert(building.navigation.revision()==oldRevision);
    assert(building.buildBarracks({-12.f,0.f,30.f}));
    assert(building.playerMinerals<oldResources);
    assert(building.navigation.revision()>oldRevision);
    const auto newFootprint=building.navigation.toCell({-12.f,0.f,30.f});
    assert(!building.navigation.walkable(newFootprint.x,newFootprint.z));

    std::cout<<"production spawns: walkable outlets, rally, destroyed blockers and depleted minerals passed\n";
}
