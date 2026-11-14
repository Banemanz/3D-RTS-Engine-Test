#include "NavigationGrid.hpp"
#include "Unit.hpp"
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    // A long static wall with an opening at the far edge. Direct steering would
    // intersect the wall; the CHASING state must route around it before attacking.
    NavigationGrid grid(13,13,1.0f,0.0f,0.0f);
    for(int z=0;z<12;++z)grid.setBlocked(6,z,true);
    UnitData ranger=UnitData::fromTresString(HUMAN_RANGER_TRES);
    UnitData target=UnitData::fromTresString(HYBRID_RAIDER_TRES);
    ranger.attack_range=1.1f;
    ranger.move_speed=7.0f;
    ranger.aggro_range=0.0f;
    target.aggro_range=0.0f;

    std::vector<Unit> army;
    army.reserve(2);
    army.emplace_back(1,"Pursuer",ranger,Vec3(2.5f,0,2.5f));
    army.emplace_back(2,"Target",target,Vec3(10.5f,0,2.5f));
    army[0].navGrid=&grid;
    army[1].ownerId=1; // ownership, not visual faction, determines hostility
    army[1].isWorker=true; // parked target, not enemy strategic AI
    army[0].attackTarget(2);
    bool traversedOpening=false;
    bool inflictedDamage=false;
    for(int step=0;step<3600;++step) {
        Vec3 before=army[0].position;
        army[0].update(1.0f/60.0f,army,100.0f);
        auto cell=grid.toCell(army[0].position);
        if(!grid.walkable(cell.x,cell.z)) {
            army[0].position=before;
            army[0].navAgent.stop();
        }
        if(army[0].position.z>11.0f)traversedOpening=true;
        if(army[1].currentHp<target.max_hp) {inflictedDamage=true;break;}
    }
    assert(traversedOpening);
    assert(inflictedDamage);
    std::cout<<"chase: tactical A* routes around a wall and reaches attack range\n";
}
