#include "Unit.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
static void duel(bool reverse) {
    UnitData d;
    d.max_hp=5.f;d.damage=100.f;d.attack_range=5.f;
    d.attack_cooldown=1.f;d.aggro_range=8.f;
    std::vector<Unit> units;
    units.emplace_back(10,"First",d,Vec3(-0.5f,0,0));
    units.emplace_back(20,"Second",d,Vec3(0.5f,0,0));
    units[0].ownerId=0;units[1].ownerId=1;
    if(reverse)std::reverse(units.begin(),units.end());
    for(auto& u:units)u.attackTarget(u.id==10?20:10);
    for(int tick=0;tick<2;++tick) {
        std::vector<CombatHit> hits;
        for(auto& unit:units)unit.update(1.f/60.f,units,48.f,nullptr,&hits);
        if(tick==0){assert(hits.empty());continue;}
        assert(hits.size()==2);
        assert(units[0].isAlive() && units[1].isAlive());
        std::sort(hits.begin(),hits.end(),[](const CombatHit& a,const CombatHit& b){
            return a.targetId!=b.targetId?a.targetId<b.targetId:a.attackerId<b.attackerId;
        });
        for(const auto& hit:hits)
            for(auto& unit:units)if(unit.id==hit.targetId)
                unit.takeDamage(hit.rawDamage,hit.attackerName);
    }
    assert(!units[0].isAlive() && !units[1].isAlive());
}
int main() {
    duel(false);duel(true);
    std::cout<<"two-phase combat: mutual kill independent of unit order passed\n";
}
