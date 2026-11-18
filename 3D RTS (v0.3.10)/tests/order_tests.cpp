#include "RTSSkirmish.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
Unit* ranger(RTSSkirmish& g) {
    for(auto& u:g.units)if(u.ownerId==0 && u.catalogId=="ranger")return &u;
    return nullptr;
}
const Unit* dummy(const RTSSkirmish& g) {
    for(const auto& u:g.units)if(u.catalogId=="test_dummy")return &u;
    return nullptr;
}
RTSProject withDummy(const Vec3& pos) {
    RTSProject p=RTSProject::defaults();
    assert(p.catalog.cloneDerived("command_center","test_dummy"));
    assert(p.catalog.setField("test_dummy","max_hp",picojson::value(4.0)));
    assert(p.catalog.setField("test_dummy","armor",picojson::value(0.0)));
    assert(p.catalog.setField("test_dummy","damage",picojson::value(0.0)));
    ProjectPlacement target;
    target.id="stationary_target";
    target.kind="Unit";target.catalogId="test_dummy";target.owner=1;
    target.name="Target Dummy";target.position=pos;
    p.placements.push_back(target);
    p.validate();
    return p;
}
void testHold() {
    RTSSkirmish game;
    game.loadProject(withDummy({-8.f,0.f,-4.5f}));
    auto* soldier=ranger(game);
    assert(soldier);
    soldier->isSelected=true;
    const int soldierId=soldier->id;
    const Vec3 start=soldier->position;
    game.orderHold();
    assert(soldier->holdPositionEnabled);
    assert(soldier->state==UnitState::IDLE);
    for(int i=0;i<180;++i)game.update(1.f/60.f);
    soldier=ranger(game);
    assert(soldier && soldier->id==soldierId && soldier->isAlive());
    assert(soldier->position.x==start.x);
    assert(soldier->position.z==start.z);
    assert(!soldier->navAgent.hasDestination);
    assert(dummy(game) && !dummy(game)->isAlive()); // attacks in range, no chase
    assert(game.orderMove({-12.f,0.f,0.f},false));
    assert(!soldier->holdPositionEnabled); // movement supersedes hold
    game.orderHold();
    assert(soldier->holdPositionEnabled);
    game.orderStop();
    assert(!soldier->holdPositionEnabled);
}
void testAttackMoveContinuesAfterCombat() {
    // Goal is a single A* cell away. The old route logic considered
    // finishing combat equivalent to reaching that waypoint, discarding it.
    RTSSkirmish game;
    game.loadProject(withDummy({-13.f,0.f,-9.5f}));
    Unit* soldier=ranger(game);
    assert(soldier);
    soldier->isSelected=true;
    const Vec3 goal{-11.f,0.f,-4.5f};
    assert(game.orderMove(goal,true));
    bool engaged=false,retookRoute=false;
    for(int i=0;i<360;++i) {
        game.update(1.f/60.f);
        soldier=ranger(game);
        assert(soldier && soldier->isAlive());
        if(soldier->state==UnitState::CHASING ||
           soldier->state==UnitState::ATTACKING)engaged=true;
        if(engaged && dummy(game) && !dummy(game)->isAlive() &&
           soldier->state==UnitState::MOVING)retookRoute=true;
    }
    assert(engaged);
    assert(dummy(game) && !dummy(game)->isAlive());
    assert(retookRoute);
    assert((soldier->position-goal).lengthXZ()<2.f);
}
}
int main() {
    testHold();
    testAttackMoveContinuesAfterCombat();
    std::cout<<"orders: hold-position fires without moving, stop clears hold, attack-move resumes after combat\n";
}
