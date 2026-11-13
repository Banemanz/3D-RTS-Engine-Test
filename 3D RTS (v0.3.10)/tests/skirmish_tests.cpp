#include "RTSSkirmish.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>

static void assertStartingState() {
    RTSSkirmish game;
    assert(game.result==MatchResult::Playing);
    assert(game.units.size()==10);
    assert(game.playerMinerals==220);
    assert(game.supplyUsed==6);
    assert(game.activeWorkerCount()==2);
    assert(game.minerals.size()==3);
}
static void assertBuildingAndProduction() {
    RTSSkirmish game;
    for(auto& u:game.units)if(u.isWorker && u.ownerId==0)u.isSelected=true;
    assert(!game.buildBarracks({-22,0,0})); // overlaps command center
    assert(game.playerMinerals==220);
    assert(game.buildBarracks({-28,0,14}));
    assert(game.playerMinerals==70);
    assert(!game.queueMarine()); // construction has not finished
    for(int i=0;i<600;++i)game.update(1.0f/60.0f);
    assert(game.completedBarracksCount()==1);
    assert(game.queueMarine());
    assert(game.playerMinerals==20);
    int previousFriendly=0;
    for(const auto& unit:game.units)if(unit.ownerId==0 && !unit.isWorker && !unit.isStructure)
        ++previousFriendly;
    for(int i=0;i<480;++i)game.update(1.0f/60.0f);
    int newFriendly=0;
    for(const auto& unit:game.units)if(unit.ownerId==0 && !unit.isWorker && !unit.isStructure)
        ++newFriendly;
    assert(newFriendly==previousFriendly+1); // enemy spawns must not mask failed production
}
static void assertOrders() {
    RTSSkirmish game;
    for(auto& u:game.units)if(u.ownerId==0 && !u.isStructure && !u.isWorker)u.isSelected=true;
    assert(game.orderMove({500,0,500},true)); // must project into playable grid
    game.update(1.0f/60.0f);
    bool hasActive=false;
    for(const auto& u:game.units)if(u.ownerId==0 && !u.isStructure && !u.isWorker) {
        assert(u.navAgent.targetPosition.x<=48.0f);
        assert(u.navAgent.targetPosition.z<=48.0f);
        hasActive=hasActive || u.navAgent.hasDestination;
    }
    assert(hasActive);
    game.orderStop();
    for(const auto& u:game.units)if(u.ownerId==0 && !u.isStructure && !u.isWorker)
        assert(!u.navAgent.hasDestination);
}
static void assertGathering() {
    RTSSkirmish game;
    for(auto& u:game.units)if(u.ownerId==0 && u.isWorker)u.isSelected=true;
    const int startRemaining=game.minerals[0].remaining;
    assert(game.orderGather(game.minerals[0].position));
    for(int i=0;i<1200;++i)game.update(1.0f/60.0f);
    assert(game.minerals[0].remaining<startRemaining);
}
static void assertDeathAndReset() {
    RTSSkirmish game;
    int before=game.supplyUsed;
    for(auto& u:game.units)if(u.ownerId==0 && !u.isWorker && !u.isStructure) {
        u.takeDamage(100000.0f,"test");
        break;
    }
    game.update(1.0f/60.0f);
    assert(game.supplyUsed==before-1);
    game.update(1.0f/60.0f);
    assert(game.supplyUsed==before-1);
    for(auto& u:game.units)if(u.ownerId==1 && u.isStructure) {
        u.takeDamage(100000.0f,"test");
        break;
    }
    game.update(1.0f/60.0f);
    assert(game.result==MatchResult::Victory);
    game.reset();
    assert(game.result==MatchResult::Playing && game.supplyUsed==6);
}
static void assertReplay() {
    RTSSkirmish a,b;
    for(auto* game:{&a,&b}) {
        for(auto& u:game->units)if(u.ownerId==0 && u.isWorker)u.isSelected=true;
        assert(game->orderGather(game->minerals[0].position));
    }
    for(int i=0;i<360;++i) {
        a.update(1.0f/60.0f);
        b.update(1.0f/60.0f);
    }
    assert(a.playerMinerals==b.playerMinerals && a.units.size()==b.units.size());
    for(size_t i=0;i<a.units.size();++i) {
        assert(a.units[i].currentHp==b.units[i].currentHp);
        assert(a.units[i].position.x==b.units[i].position.x);
        assert(a.units[i].position.z==b.units[i].position.z);
    }
}
static void assertQueuedMovement() {
    RTSSkirmish game;
    int workerId=-1;
    for(auto& u:game.units)if(u.isWorker && u.ownerId==0 && workerId==-1) {
        workerId=u.id;u.isSelected=true;
    }
    assert(workerId!=-1);
    assert(game.orderMove({-12,0,-10},false));
    assert(game.orderMove({-12,0,-2},false,true));
    for(int i=0;i<900;++i)game.update(1.0f/60.0f);
    bool close=false;
    for(const auto& u:game.units)if(u.id==workerId)
        close=u.isAlive() && (u.position-Vec3(-12,0,-2)).lengthXZ()<3.0f;
    assert(close);
}
static void assertPatrol() {
    RTSProject map=RTSProject::defaults();
    map.placements.erase(std::remove_if(map.placements.begin(),map.placements.end(),
       [](const ProjectPlacement& p){return p.owner==1 && p.catalogId=="zealot";}),
       map.placements.end());
    RTSSkirmish game;
    game.loadProject(map);
    int id=-1;
    for(auto& unit:game.units)if(unit.ownerId==0 && unit.isWorker && id==-1) {
        id=unit.id;
        unit.isSelected=true;
    }
    assert(id!=-1);
    assert(game.orderPatrol({-10,0,-5.5f}));
    bool outbound=false,returned=false;
    for(int i=0;i<720;++i) {
        game.update(1.f/60.f);
        for(const auto& unit:game.units)if(unit.id==id) {
            if(unit.position.x>-12.5f)outbound=true;
            if(outbound && unit.position.x<-16.f)returned=true;
        }
    }
    assert(outbound && returned);
    game.orderStop();
    for(const auto& unit:game.units)if(unit.id==id)
        assert(!unit.navAgent.hasDestination);
}
int main() {
    assertStartingState();
    assertBuildingAndProduction();
    assertOrders();
    assertGathering();
    assertDeathAndReset();
    assertReplay();
    assertQueuedMovement();
    assertPatrol();
    std::cout<<"skirmish: 8 tests passed\n";
}
