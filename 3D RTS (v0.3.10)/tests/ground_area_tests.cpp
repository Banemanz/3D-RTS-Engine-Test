#include "RTSSkirmish.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Ground impact tests require active assertions in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>

namespace {
void near(float actual,float expected){assert(std::fabs(actual-expected)<.02f);}
CatalogEntry effect(const char* id,const char* kind,double amount,
                    bool periodic=false){
    CatalogEntry e;e.id=id;e.type="Effect";
    e.fields={{"kind",picojson::value(kind)},
              {"amount",picojson::value(amount)}};
    if(periodic){
        e.fields["interval_ticks"]=picojson::value(120.0);
        e.fields["duration_ticks"]=picojson::value(600.0);
    }
    return e;
}
CatalogEntry ground(const char* id,const char* effectId,
                    const char* team,int delay,double radius=3.){
    CatalogEntry e;e.id=id;e.type="Ability";
    e.fields={{"name",picojson::value(id)},
        {"target",picojson::value("Ground")},
        {"effect",picojson::value(effectId)},
        {"area_radius",picojson::value(radius)},
        {"area_team",picojson::value(team)},
        {"impact_delay_ticks",picojson::value(double(delay))},
        {"cooldown_ticks",picojson::value(9.0)},
        {"cast_range",picojson::value(14.0)},
        {"energy_cost",picojson::value(5.0)},
        {"mineral_cost",picojson::value(7.0)}};
    return e;
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
float health(const RTSSkirmish& game,int id){
    for(const auto& u:game.units)if(u.id==id)return u.currentHp;
    return -1.f;
}
Unit* entity(RTSSkirmish& game,int id){
    for(auto& u:game.units)if(u.id==id)return &u;
    return nullptr;
}
int firstUnit(const RTSSkirmish& game,const std::string& catalogId){
    for(const auto& u:game.units)
        if(u.catalogId==catalogId)return u.id;
    return -1;
}
RTSProject projectFixture(){
    RTSProject p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    assert(p.catalog.add(effect("blast_hit","Damage",12.0)));
    assert(p.catalog.add(effect("area_restore","Heal",20.0)));
    assert(p.catalog.add(effect("safe_pulse","PeriodicDamage",2.0,true)));
    assert(p.catalog.add(ground("ground_burst","blast_hit","Enemy",3)));
    assert(p.catalog.add(ground("area_mend","area_restore","Ally",0)));
    assert(p.catalog.add(ground("instant_pulse","safe_pulse","Enemy",0)));
    assert(p.catalog.setField("ranger","max_energy",picojson::value(200.0)));
    assert(p.catalog.setField("ranger","energy_regen",picojson::value(0.0)));
    // Keep combat inert so all damage assertions exercise only the area Effect.
    assert(p.catalog.setField("ranger","damage",picojson::value(0.0)));
    assert(p.catalog.setField("ranger","aggro_range",picojson::value(0.0)));
    assert(p.catalog.setField("ranger","attack_range",picojson::value(0.0)));
    assert(p.catalog.setField("ranger","abilities",picojson::value(
        picojson::array{picojson::value("ground_burst"),
                        picojson::value("area_mend"),
                        picojson::value("instant_pulse")})));
    assert(p.catalog.cloneDerived("rift_raider","area_dummy"));
    for(const char* k:{"damage","armor","aggro_range","move_speed"}){
        assert(p.catalog.setField("area_dummy",k,picojson::value(0.0)));
    }
    assert(p.catalog.setField("area_dummy","max_hp",picojson::value(500.0)));
    const Vec3 locations[]={{-6.f,0.f,-4.5f},{-8.f,0.f,-4.5f},
                            {-2.f,0.f,-4.5f}};
    for(int i=0;i<3;++i){
        ProjectPlacement placed;
        placed.id="ground_target_"+std::to_string(i);
        placed.kind="Unit";
        placed.catalogId="area_dummy";
        placed.name="Original Target "+std::to_string(i);
        placed.owner=1;
        placed.position=locations[i];
        p.placements.push_back(std::move(placed));
    }
    p.validate();
    return p;
}
}
int main(){
    RTSProject project=projectFixture();
    const Vec3 center{-7.f,0.f,-4.5f};
    const auto definition=compileAbility(project.catalog,"ground_burst");
    assert(definition.target==AbilityTarget::Ground);
    assert(definition.areaTeam==AbilityAreaTeam::Enemy);
    assert(definition.areaRadius==3.f && definition.impactDelayTicks==3);
    const RTSProject saved=RTSProject::fromJson(project.toJson());
    assert(compileAbility(saved.catalog,"ground_burst").areaRadius==3.f);

    RTSSkirmish game;game.loadProject(saved);
    const int caster=firstUnit(game,"ranger");
    assert(caster>0);
    for(auto& u:game.units)u.isSelected=(u.id==caster);
    int dummy[3]={-1,-1,-1},index=0;
    for(const auto& u:game.units)if(u.catalogId=="area_dummy"){
        assert(index<3);dummy[index++]=u.id;
    }
    assert(index==3);
    assert(game.vision.visible(center));
    assert(game.canPrepareAbility(caster,"ground_burst"));
    assert(game.canCastAbilityAt(caster,"ground_burst",center));
    assert(!game.canCastAbility(caster,"ground_burst",dummy[0]));
    assert(!game.canCastAbilityAt(caster,"ground_burst",{38.f,0.f,38.f}));
    assert(!game.canCastAbilityAt(caster,"area_mend",{10000.f,0.f,0.f}));
    const int beforeMinerals=game.playerMinerals;
    const float enemyHp[3]={health(game,dummy[0]),health(game,dummy[1]),
                            health(game,dummy[2])};
    const float casterHp=health(game,caster);
    std::ofstream fingerprint("ground_area_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<game.stateDigest()<<"\n";
    // A tampered command with an explicit Unit target is rejected on tick,
    // even though the catalog target is Ground.
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,
                                   center,dummy[0],false,"ground_burst"));
    game.update(1.f/60.f); // tick 1: command denied
    assert(game.takeCommandOutcomes()[0].accepted==false);
    assert(game.pendingAreaImpactCount()==0);
    assert(game.playerMinerals==beforeMinerals);
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,
                                   center,-1,false,"ground_burst"));
    game.update(1.f/60.f); // tick 2: delayed cast accepted
    assert(game.pendingAreaImpactCount()==1);
    assert(game.areaImpactWatch()[0].remainingTicks==3);
    assert(game.abilityCooldownRemaining(caster,"ground_burst")==9);
    assert(game.playerMinerals==beforeMinerals-7);
    near(entity(game,caster)->currentEnergy,195.f);
    for(int i=0;i<3;++i)near(health(game,dummy[i]),enemyHp[i]);
    fingerprint<<"2 "<<game.stateDigest()<<"\n";
    game.update(1.f/60.f); // tick 3
    game.update(1.f/60.f); // tick 4
    assert(game.pendingAreaImpactCount()==1);
    near(health(game,dummy[0]),enemyHp[0]);
    game.update(1.f/60.f); // tick 5: arrival after movement
    assert(game.pendingAreaImpactCount()==0);
    near(health(game,dummy[0]),enemyHp[0]-12.f);
    near(health(game,dummy[1]),enemyHp[1]-12.f);
    near(health(game,dummy[2]),enemyHp[2]); // outside radius
    near(health(game,caster),casterHp); // enemy-only filter
    fingerprint<<"5 "<<game.stateDigest()<<"\n";
    for(int tick=6;tick<=35;++tick){
        if(tick==14)assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,
                                                  center,-1,false,"ground_burst"));
        game.update(1.f/60.f);
        if(tick%5==0)fingerprint<<tick<<" "<<game.stateDigest()<<"\n";
    }
    near(health(game,dummy[0]),enemyHp[0]-24.f);
    near(health(game,dummy[1]),enemyHp[1]-24.f);
    const auto tape=game.replayTape();
    assert(tape.commands.size()==3); // one rejected, two accepted casts
    assert(tape.commands[1].point.x==center.x &&
           tape.commands[1].targetId==-1);
    const auto decoded=commandFromJson(commandToJson(tape.commands[1]));
    assert(decoded.point.z==center.z);
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&diagnostic));
    auto tampered=tape;
    tampered.commands[1].point.x+=8.f;
    assert(!verifyRTSReplay(tampered,&diagnostic));

    // A unit may dodge *after cast but before impact*, and there is no
    // renderer-dependent target cache at cast time.
    RTSSkirmish dodged;dodged.loadProject(saved);
    for(auto& u:dodged.units)u.isSelected=(u.id==caster);
    assert(dodged.queuePlayerCommand(RTSCommandKind::CastAbility,
                                     center,-1,false,"ground_burst"));
    dodged.update(1.f/60.f);
    const float hpBeforeDodge=health(dodged,dummy[0]);
    entity(dodged,dummy[0])->position.x=-15.f; // test-only external reposition
    for(int i=0;i<3;++i)dodged.update(1.f/60.f);
    near(health(dodged,dummy[0]),hpBeforeDodge);
    near(health(dodged,dummy[1]),enemyHp[1]-12.f);

    // Healing ground area respects authored Ally team filter.
    RTSSkirmish medic;medic.loadProject(saved);
    for(auto& u:medic.units)u.isSelected=(u.id==caster);
    entity(medic,caster)->currentHp-=15.f;
    assert(medic.canCastAbilityAt(caster,"area_mend",
                                  entity(medic,caster)->position));
    assert(medic.queuePlayerCommand(RTSCommandKind::CastAbility,
                 entity(medic,caster)->position,-1,false,"area_mend"));
    medic.update(1.f/60.f);
    near(health(medic,caster),casterHp);
    near(health(medic,dummy[0]),enemyHp[0]);

    // Multi-target scheduling admission cannot partially apply a Sequence
    // when the global periodic queue has only one free slot.
    RTSSkirmish saturated;saturated.loadProject(saved);
    for(auto& u:saturated.units)u.isSelected=(u.id==caster);
    for(int i=0;i<255;++i)
        assert(saturated.applyEffectForTrigger("safe_pulse",caster));
    assert(saturated.activePeriodicEffectCount()==255);
    const int mineralsBefore=saturated.playerMinerals;
    const float energyBefore=entity(saturated,caster)->currentEnergy;
    assert(saturated.queuePlayerCommand(RTSCommandKind::CastAbility,
                                         center,-1,false,"instant_pulse"));
    saturated.update(1.f/60.f);
    assert(!saturated.takeCommandOutcomes()[0].accepted);
    assert(saturated.activePeriodicEffectCount()==255);
    assert(saturated.playerMinerals==mineralsBefore);
    near(entity(saturated,caster)->currentEnergy,energyBefore);
    saturated.reset();
    assert(saturated.pendingAreaImpactCount()==0);

    RTSSkirmish crowded;crowded.loadProject(saved);
    for(int i=0;i<65;++i)
        assert(crowded.spawnForTrigger("area_dummy",1,center));
    for(auto& u:crowded.units)u.isSelected=(u.id==caster);
    assert(crowded.queuePlayerCommand(RTSCommandKind::CastAbility,
                                      center,-1,false,"ground_burst"));
    crowded.update(1.f/60.f);
    assert(crowded.pendingAreaImpactCount()==1);
    for(int i=0;i<3;++i)crowded.update(1.f/60.f);
    assert(crowded.pendingAreaImpactCount()==0);
    near(health(crowded,dummy[0]),enemyHp[0]); // 64 victim hard cap fizzles

    for(const auto& [key,bad]:{
        std::pair<const char*,double>{"area_radius",.49},
        {"area_radius",24.01},{"impact_delay_ticks",-1.},
        {"impact_delay_ticks",301.},{"impact_delay_ticks",2.5}
    }){
        auto broken=project;
        assert(broken.catalog.setField("ground_burst",key,picojson::value(bad)));
        assert(invalid(broken));
    }
    auto broken=project;
    assert(broken.catalog.setField("ground_burst","area_team",picojson::value("Neutral")));
    assert(invalid(broken));
    broken=project;
    assert(broken.catalog.setField("ground_burst","area_team",picojson::value(3.0)));
    assert(invalid(broken));
    broken=project;
    assert(broken.catalog.setField("ground_burst","cast_range",picojson::value(0.0)));
    assert(invalid(broken));
    broken=project;
    assert(broken.catalog.setField("ground_burst","target",picojson::value("Terrain")));
    assert(invalid(broken));
    fingerprint.flush();assert(fingerprint.good());
    std::cout<<"ground area: delayed impacts, enemy/ally filters, out-of-radius dodge, bounded atomic admission, replay passed\n";
}
