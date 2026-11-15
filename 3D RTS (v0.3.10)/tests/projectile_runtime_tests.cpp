#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cassert>
#if defined(NDEBUG)
#error "Projectile regression requires asserts in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>

namespace {
CatalogEntry effect(const char* id,const char* kind,double amount){
    CatalogEntry e;e.id=id;e.type="Effect";
    e.fields={{"kind",picojson::value(kind)},{"amount",picojson::value(amount)}};
    return e;
}
CatalogEntry projectile(const char* id,float speed,float arc,bool collision){
    CatalogEntry e;e.id=id;e.type="Projectile";
    e.fields={{"name",picojson::value(id)},
       {"speed",picojson::value(double(speed))},
       {"arc_height",picojson::value(double(arc))},
       {"visual_scale",picojson::value(.7)},
       {"collision_radius",picojson::value(.4)},
       {"collide_units",picojson::value(collision)},
       {"tint",picojson::value(picojson::array{
           picojson::value(.2),picojson::value(.8),picojson::value(1.)})}};
    return e;
}
CatalogEntry ability(const char* id,const char* projectileId,int delay){
    CatalogEntry e;e.id=id;e.type="Ability";
    e.fields={{"name",picojson::value(id)},
      {"target",picojson::value("Ground")},
      {"effect",picojson::value("impact_damage")},
      {"projectile",picojson::value(projectileId)},
      {"area_radius",picojson::value(1.3)},
      {"area_team",picojson::value("Enemy")},
      {"impact_delay_ticks",picojson::value(double(delay))},
      {"cast_range",picojson::value(12.)},
      {"cooldown_ticks",picojson::value(1.)},
      {"mineral_cost",picojson::value(4.)},
      {"energy_cost",picojson::value(5.)}};
    return e;
}
RTSProject makeProject(){
    RTSProject p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.);
    assert(p.catalog.add(effect("impact_damage","Damage",12)));
    assert(p.catalog.add(projectile("arc_spark",12.f,2.f,false)));
    assert(p.catalog.add(projectile("contact_bolt",12.f,0.f,true)));
    assert(p.catalog.add(ability("arc_impact","arc_spark",2)));
    assert(p.catalog.add(ability("bolt_impact","contact_bolt",0)));
    assert(p.catalog.setField("ranger","max_energy",picojson::value(100.)));
    assert(p.catalog.setField("ranger","energy_regen",picojson::value(0.)));
    assert(p.catalog.setField("ranger","damage",picojson::value(0.)));
    assert(p.catalog.setField("ranger","aggro_range",picojson::value(0.)));
    assert(p.catalog.setField("ranger","attack_range",picojson::value(0.)));
    assert(p.catalog.setField("ranger","abilities",picojson::value(
        picojson::array{picojson::value("arc_impact"),
                        picojson::value("bolt_impact")})));
    assert(p.catalog.setField("command_center","abilities",
                              picojson::value(picojson::array{})));
    assert(p.catalog.cloneDerived("rift_raider","shot_dummy"));
    for(const char* field:{"damage","armor","move_speed","aggro_range"}){
        assert(p.catalog.setField("shot_dummy",field,picojson::value(0.)));
    }
    assert(p.catalog.setField("shot_dummy","max_hp",picojson::value(500.)));
    for(int i=0;i<2;++i){
        ProjectPlacement place;
        place.kind="Unit";place.id=i?"far_dummy":"near_dummy";
        place.catalogId="shot_dummy";place.name=place.id;
        place.owner=1;place.position={i?-6.f:-9.f,0.f,-4.5f};
        p.placements.push_back(std::move(place));
    }
    p.validate();
    return p;
}
int casterId(const RTSSkirmish& game){
    for(const auto& u:game.units)
        if(u.catalogId=="ranger"&&u.ownerId==0)return u.id;
    return -1;
}
std::vector<int> dummyIds(const RTSSkirmish& game){
    std::vector<std::pair<float,int>> sorted;
    for(const auto& u:game.units)
        if(u.catalogId=="shot_dummy")
            sorted.push_back({u.position.x,u.id});
    std::sort(sorted.begin(),sorted.end());
    std::vector<int> ids;
    for(const auto& pair:sorted)ids.push_back(pair.second);
    return ids;
}
float hp(const RTSSkirmish& game,int id){
    for(const auto& u:game.units)if(u.id==id)return u.currentHp;
    return -1.f;
}
void near(float a,float b){assert(std::fabs(a-b)<.02f);}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
void select(RTSSkirmish& game,int id){
    for(auto& u:game.units)u.isSelected=u.id==id;
}
}
int main(){
    RTSProject p=makeProject();
    const auto original=compileProjectile(p.catalog,"arc_spark");
    assert(original.speed==12.f && original.arcHeight==2.f &&
           !original.collideUnits);
    assert(compileAbility(p.catalog,"arc_impact").projectileId=="arc_spark");
    const RTSProject decoded=RTSProject::fromJson(p.toJson());
    RTSSkirmish game;game.loadProject(decoded);
    const int caster=casterId(game);assert(caster>0);
    const auto ids=dummyIds(game);assert(ids.size()==2);
    const int nearDummy=ids[0],farDummy=ids[1];
    const Vec3 goal{-6.f,0.f,-4.5f};
    select(game,caster);
    assert(game.canCastAbilityAt(caster,"arc_impact",goal));
    const int mineralBefore=game.playerMinerals;
    const float hpNear=hp(game,nearDummy),hpFar=hp(game,farDummy);
    std::ofstream fingerprint("projectile_fingerprint.txt",std::ios::binary);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<game.stateDigest()<<"\n";
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,
                                   goal,-1,false,"arc_impact"));
    game.update(1.f/60.f);
    assert(game.takeCommandOutcomes().at(0).accepted);
    assert(game.playerMinerals==mineralBefore-4);
    assert(game.pendingAreaImpactCount()==1);
    auto watch=game.areaImpactWatch();
    assert(watch.size()==1 && watch[0].projectileVisible &&
           watch[0].projectileScale>.2f);
    assert(watch[0].remainingTicks>2 && watch[0].remainingTicks<120);
    const std::uint64_t remaining=watch[0].remainingTicks;
    const Vec3 initial=watch[0].projectilePosition;
    fingerprint<<"1 "<<game.stateDigest()<<"\n";
    for(std::uint64_t n=1;n<remaining;++n){
        game.update(1.f/60.f);
        if(n==remaining/2){
            watch=game.areaImpactWatch();
            assert(watch.size()==1 && watch[0].projectileVisible);
            assert(watch[0].projectilePosition.x>initial.x+1.f);
            assert(watch[0].projectilePosition.y>initial.y);
            fingerprint<<"middle "<<game.stateDigest()<<"\n";
        }
        if(n%12==0)fingerprint<<n+1<<" "<<game.stateDigest()<<"\n";
    }
    assert(game.pendingAreaImpactCount()==1);
    game.update(1.f/60.f);
    assert(game.pendingAreaImpactCount()==0);
    assert(!game.impactFlashWatch().empty());
    near(hp(game,nearDummy),hpNear);
    near(hp(game,farDummy),hpFar-12.f);
    fingerprint<<"end "<<game.stateDigest()<<"\n";
    for(int i=0;i<25;++i)game.update(1.f/60.f);
    assert(game.impactFlashWatch().empty());
    fingerprint<<"after "<<game.stateDigest()<<"\n";
    fingerprint.flush();assert(fingerprint.good());
    const auto tape=game.replayTape();
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&diagnostic));
    auto tampered=tape;
    tampered.commands[0].point.x+=2.f;
    assert(!verifyRTSReplay(tampered,&diagnostic));

    // Swept unit contact interrupts the flight instead of waiting for the
    // planned destination and centers the burst on the intercepted victim.
    RTSSkirmish intercept;intercept.loadProject(decoded);
    select(intercept,caster);
    assert(intercept.queuePlayerCommand(RTSCommandKind::CastAbility,
                                        goal,-1,false,"bolt_impact"));
    intercept.update(1.f/60.f);
    const std::uint64_t originalRemaining=
        intercept.areaImpactWatch()[0].remainingTicks;
    bool hit=false;
    for(std::uint64_t n=0;n<originalRemaining;++n){
        intercept.update(1.f/60.f);
        if(intercept.pendingAreaImpactCount()==0){hit=true;break;}
    }
    assert(hit);
    near(hp(intercept,nearDummy),hpNear-12.f);
    near(hp(intercept,farDummy),hpFar); // intercepted before destination
    assert(intercept.impactFlashWatch()[0].applied);

    // Target moves out after launch: no persistent target tracking is stored.
    RTSSkirmish dodge;dodge.loadProject(decoded);select(dodge,caster);
    assert(dodge.queuePlayerCommand(RTSCommandKind::CastAbility,
                                    goal,-1,false,"arc_impact"));
    dodge.update(1.f/60.f);
    const auto until=dodge.areaImpactWatch()[0].remainingTicks;
    for(auto& u:dodge.units)if(u.id==farDummy)u.position.x=-1.f;
    for(std::uint64_t t=0;t<until;++t)dodge.update(1.f/60.f);
    near(hp(dodge,farDummy),hpFar);

    // Typed reference integrity and invalid field validation.
    std::string error;
    assert(!p.removeCatalogObject("arc_spark",&error));
    assert(error.find("projectile")!=std::string::npos);
    assert(p.renameCatalogObject("arc_spark","new_arc"));
    assert(compileAbility(p.catalog,"arc_impact").projectileId=="new_arc");
    for(const auto& bad:std::vector<std::pair<std::string,double>>{
        {"speed",0.},{"arc_height",25.},{"collision_radius",0.},
        {"visual_scale",4.}}){
        RTSProject broken=p;
        assert(broken.catalog.setField("new_arc",bad.first,picojson::value(bad.second)));
        assert(invalid(broken));
    }
    RTSProject broken=p;
    assert(broken.catalog.setField("new_arc","collide_units",
                                   picojson::value("true")));
    assert(invalid(broken));
    broken=p;
    assert(broken.catalog.setField("arc_impact","projectile",
                                   picojson::value("missing_ref")));
    assert(invalid(broken));
    broken=p;
    assert(broken.catalog.setField("arc_impact","target",
                                   picojson::value("Self")));
    assert(invalid(broken));
    std::cout<<"Projectile speed/arc, midflight render positions, swept interception, dodge, replay, typed refs passed\n";
}
