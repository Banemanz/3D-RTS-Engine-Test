#include "RTSSkirmish.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Weapon runtime assertions must work in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
CatalogEntry rifle(){
    CatalogEntry weapon;weapon.id="piercer_rifle";weapon.type="Weapon";
    weapon.fields={{"name",picojson::value("Piercer Rifle")},
        {"damage",picojson::value(15.)},
        {"range",picojson::value(8.)},
        {"cooldown",picojson::value(.5)},
        {"armor_piercing",picojson::value(5.)}};
    return weapon;
}
RTSProject fixture(){
    auto p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.);
    assert(p.catalog.add(rifle()));
    assert(p.catalog.setField("ranger","weapon",picojson::value("piercer_rifle")));
    assert(p.catalog.setField("ranger","aggro_range",picojson::value(0.)));
    assert(p.catalog.cloneDerived("rift_raider","armored_dummy"));
    for(const char* key:{"damage","attack_range","aggro_range","move_speed"})
        assert(p.catalog.setField("armored_dummy",key,picojson::value(0.)));
    assert(p.catalog.setField("armored_dummy","armor",picojson::value(8.)));
    assert(p.catalog.setField("armored_dummy","max_hp",picojson::value(500.)));
    ProjectPlacement placed;
    placed.id="weapon_target";placed.kind="Unit";placed.catalogId="armored_dummy";
    placed.name="Armored Target";placed.owner=1;placed.position={-8.f,0.f,-4.5f};
    p.placements.push_back(std::move(placed));
    p.validate();return p;
}
}
int main(){
    RTSProject p=fixture();
    const auto weapon=compileWeapon(p.catalog,"piercer_rifle");
    assert(weapon.damage==15.f && weapon.range==8.f &&
           weapon.cooldown==.5f && weapon.armorPiercing==5.f);
    const UnitData shooterData=unitFromCatalog(p.catalog,"ranger");
    assert(shooterData.weaponId=="piercer_rifle");
    assert(shooterData.damage==15.f && shooterData.attack_range==8.f &&
           shooterData.attack_cooldown==.5f && shooterData.armor_piercing==5.f);
    // The normal attack uses the weapon's penetration, while generic
    // direct Effect damage retains full armor mitigation.
    UnitData armored=unitFromCatalog(p.catalog,"armored_dummy");
    Unit pierced(1,"Pierced",armored,{0.f,0.f,0.f});
    Unit ordinary(2,"Ordinary",armored,{0.f,0.f,0.f});
    pierced.takeDamage(15.f,"Weapon",5.f);
    ordinary.takeDamage(15.f,"Effect");
    assert(std::fabs(pierced.currentHp-(armored.max_hp-12.f))<.01f);
    assert(std::fabs(ordinary.currentHp-(armored.max_hp-7.f))<.01f);
    // Stat behavior armor still participates before penetration.
    pierced.behaviorArmorBonus=4.f;
    pierced.takeDamage(15.f,"Weapon",5.f);
    assert(std::fabs(pierced.currentHp-(armored.max_hp-20.f))<.01f);
    // Two-phase Unit::update carries the authored piercing stat into CombatHit.
    std::vector<Unit> duel;
    duel.emplace_back(10,"Attacker",shooterData,Vec3(-2.f,0.f,0.f));
    duel.emplace_back(20,"Defender",armored,Vec3(2.f,0.f,0.f));
    duel[0].ownerId=0;duel[1].ownerId=1;
    duel[0].attackTarget(20);
    std::vector<CombatHit> hits;
    duel[0].update(1.f/60.f,duel,48.f,nullptr,&hits);
    duel[0].update(1.f/60.f,duel,48.f,nullptr,&hits);
    assert(hits.size()==1);
    assert(hits[0].attackerId==10 && hits[0].targetId==20 &&
           hits[0].rawDamage==15.f && hits[0].armorPiercing==5.f);

    // Combat commands, not direct Unit mutation, drive the canonical tape.
    RTSSkirmish game;game.loadProject(RTSProject::fromJson(p.toJson()));
    int caster=-1,target=-1;
    for(auto& unit:game.units){
        if(caster<0 && unit.catalogId=="ranger" && unit.ownerId==0){
            caster=unit.id;unit.isSelected=true;
        }
        if(unit.catalogId=="armored_dummy")target=unit.id;
    }
    assert(caster>0 && target>0);
    float initial=-1.f;
    for(const auto& unit:game.units)if(unit.id==target)initial=unit.currentHp;
    assert(initial==500.f);
    std::ofstream out("weapon_fingerprint.txt",
                      std::ios::binary|std::ios::trunc);
    assert(out.is_open());
    out<<"0 "<<game.stateDigest()<<"\n";
    assert(game.queuePlayerCommand(RTSCommandKind::Attack,{},target));
    for(int i=1;i<=80;++i){
        game.update(1.f/60.f);
        if(i==2){
            float hp=-1.f;
            for(const auto& unit:game.units)if(unit.id==target)hp=unit.currentHp;
            assert(std::fabs(hp-(initial-12.f))<.01f);
        }
        if(i%10==0)out<<i<<" "<<game.stateDigest()<<"\n";
    }
    out.flush();assert(out.good());
    const auto tape=game.replayTape();
    assert(tape.commands.size()==1 && tape.commands[0].kind==RTSCommandKind::Attack);
    std::string why;
    assert(verifyRTSReplay(tape,&why));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&why));
    auto tampered=tape;
    tampered.commands[0].targetId=caster;
    assert(!verifyRTSReplay(tampered,&why));

    // Strict typed refs, bounds and backward-compatible unarmed Unit stats.
    for(const auto& bad:std::vector<std::pair<const char*,double>>{
        {"damage",-1.},{"range",129.},{"cooldown",0.},
        {"armor_piercing",-1.},{"armor_piercing",1001.}}){
        auto broken=p;
        assert(broken.catalog.setField("piercer_rifle",bad.first,
                                       picojson::value(bad.second)));
        assert(invalid(broken));
    }
    auto missing=p;
    assert(missing.catalog.setField("ranger","weapon",
                                    picojson::value("absent_weapon")));
    assert(invalid(missing));
    auto old=p;
    assert(old.catalog.setField("ranger","weapon",picojson::value("")));
    old.validate();
    assert(unitFromCatalog(old.catalog,"ranger").damage==7.f);
    assert(unitFromCatalog(old.catalog,"ranger").armor_piercing==0.f);
    std::cout<<"Authored Weapon, armor penetration, fixed-tick combat/replay and legacy Unit compatibility passed\n";
}
