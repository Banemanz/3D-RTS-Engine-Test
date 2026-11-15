#include "RTSSkirmish.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Weapon Projectile tests require enabled Release assertions"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
CatalogEntry projectile(bool intercept){
    CatalogEntry e;e.type="Projectile";e.id="practice_bolt";
    e.fields={{"speed",picojson::value(6.)},
              {"arc_height",picojson::value(0.)},
              {"collision_radius",picojson::value(.4)},
              {"visual_scale",picojson::value(.35)},
              {"collide_units",picojson::value(intercept)}};
    return e;
}
CatalogEntry weapon(){
    CatalogEntry e;e.type="Weapon";e.id="practice_launcher";
    e.fields={{"damage",picojson::value(20.)},
              {"range",picojson::value(12.)},
              {"cooldown",picojson::value(10.)},
              {"armor_piercing",picojson::value(3.)},
              {"projectile",picojson::value("practice_bolt")}};
    return e;
}
RTSProject makeProject(bool intercept=false,bool blocker=false){
    auto p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.);
    assert(p.catalog.add(projectile(intercept)));
    assert(p.catalog.add(weapon()));
    assert(p.catalog.setField("ranger","weapon",
                              picojson::value("practice_launcher")));
    assert(p.catalog.setField("ranger","aggro_range",picojson::value(0.)));
    assert(p.catalog.cloneDerived("rift_raider","weapon_test_dummy"));
    for(const char* field:{"damage","attack_range","aggro_range","move_speed"})
        assert(p.catalog.setField("weapon_test_dummy",field,picojson::value(0.)));
    assert(p.catalog.setField("weapon_test_dummy","armor",picojson::value(8.)));
    assert(p.catalog.setField("weapon_test_dummy","max_hp",picojson::value(500.)));
    ProjectPlacement target;target.id="main_dummy";target.kind="Unit";
    target.catalogId="weapon_test_dummy";target.name="Main Dummy";
    target.owner=1;target.position={-8.f,0.f,-4.5f};
    p.placements.push_back(target);
    if(blocker){
        ProjectPlacement other=target;
        other.id="intercept_dummy";other.name="Interceptor Dummy";
        other.position={-10.5f,0.f,-4.5f};
        p.placements.push_back(other);
    }
    p.validate();return p;
}
int locate(const RTSSkirmish& game,const std::string& name){
    for(const auto& u:game.units)if(u.instanceName==name)return u.id;
    return -1;
}
float hp(const RTSSkirmish& game,int id){
    for(const auto& u:game.units)if(u.id==id)return u.currentHp;
    return -1.f;
}
int selectShooter(RTSSkirmish& game){
    int id=-1;
    for(const auto& u:game.units)
        if(id<0 && u.ownerId==0 && u.catalogId=="ranger")id=u.id;
    for(auto& u:game.units)u.isSelected=u.id==id;
    return id;
}
void fire(RTSSkirmish& game,int target){
    assert(game.queuePlayerCommand(RTSCommandKind::Attack,{},target));
    game.update(1.f/60.f);
    game.update(1.f/60.f);
    assert(game.pendingWeaponShotCount()==1);
}
}
int main(){
    auto p=makeProject();
    assert(compileWeapon(p.catalog,"practice_launcher").projectileId=="practice_bolt");
    assert(unitFromCatalog(p.catalog,"ranger").projectileId=="practice_bolt");
    RTSSkirmish game;
    game.loadProject(RTSProject::fromJson(p.toJson()));
    assert(selectShooter(game)>0);
    const int target=locate(game,"Main Dummy");
    assert(target>0 && hp(game,target)==500.f);
    std::ofstream fingerprint("weapon_projectile_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<game.stateDigest()<<"\n";
    fire(game,target);
    const auto inFlight=game.areaImpactWatch();
    assert(inFlight.size()==1);
    assert(inFlight[0].weaponShot && inFlight[0].projectileVisible &&
           inFlight[0].remainingTicks>10);
    assert(hp(game,target)==500.f); // cannot hit instantly
    fingerprint<<"2 "<<game.stateDigest()<<"\n";
    for(int tick=3;tick<=150;++tick){
        game.update(1.f/60.f);
        if(tick%15==0)
            fingerprint<<tick<<" "<<game.stateDigest()<<"\n";
    }
    assert(game.pendingWeaponShotCount()==0);
    assert(hp(game,target)==485.f); // 20 damage, 8 armor, 3 piercing
    fingerprint.flush();assert(fingerprint.good());
    const auto tape=game.replayTape();
    assert(tape.commands.size()==1 &&
           tape.commands[0].kind==RTSCommandKind::Attack);
    std::string error;
    assert(verifyRTSReplay(tape,&error));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&error));
    auto tampered=tape;tampered.commands[0].targetId=1;
    assert(!verifyRTSReplay(tampered,&error));

    // Untracked direct motion changes invalidate any interpretation of
    // this separate dodge experiment as a replay; no parity file uses it.
    RTSSkirmish dodge;dodge.loadProject(p);
    assert(selectShooter(dodge)>0);
    const int dodgeId=locate(dodge,"Main Dummy");
    fire(dodge,dodgeId);
    for(auto& u:dodge.units)if(u.id==dodgeId)
        u.position.z+=10.f;
    for(int tick=0;tick<150;++tick)dodge.update(1.f/60.f);
    assert(dodge.pendingWeaponShotCount()==0);
    assert(hp(dodge,dodgeId)==500.f);

    // Interception reuses the normal Ability projectile's swept collider
    // and stable first-contact/Unit-ID priority.
    const RTSProject interceptProject=makeProject(true,true);
    RTSSkirmish intercepted;intercepted.loadProject(interceptProject);
    assert(selectShooter(intercepted)>0);
    const int far=locate(intercepted,"Main Dummy");
    const int near=locate(intercepted,"Interceptor Dummy");
    assert(far>0 && near>0);
    fire(intercepted,far);
    for(int tick=0;tick<150;++tick)intercepted.update(1.f/60.f);
    assert(hp(intercepted,far)==500.f);
    assert(hp(intercepted,near)==485.f);
    assert(intercepted.pendingWeaponShotCount()==0);

    auto bad=p;
    assert(bad.catalog.setField("practice_launcher","projectile",
                                picojson::value("unknown_bolt")));
    bool rejected=false;
    try{bad.validate();}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    auto malformed=p;
    assert(malformed.catalog.setField("practice_launcher","projectile",
                                      picojson::value(3.)));
    rejected=false;
    try{malformed.validate();}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::cout<<"Weapon flight, dodge, interception, replay and typed validation passed\n";
}
