#include "RTSSkirmish.hpp"
#include "RTSCommandStream.hpp"
#include <algorithm>
#include <cassert>
#if defined(NDEBUG)
#error "Behavior runtime regression requires active asserts in Release builds"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
CatalogEntry behavior(const char* id,double duration=6.,int stacks=3,
                      double armor=4.,double damage=2.,double speed=.75,
                      bool refresh=true,bool dispellable=true){
    CatalogEntry entry;entry.id=id;entry.type="Behavior";
    entry.fields={{"name",picojson::value(id)},
        {"duration_ticks",picojson::value(duration)},
        {"max_stacks",picojson::value(double(stacks))},
        {"armor_bonus",picojson::value(armor)},
        {"damage_bonus",picojson::value(damage)},
        {"speed_multiplier",picojson::value(speed)},
        {"refresh_duration",picojson::value(refresh)},
        {"dispellable",picojson::value(dispellable)}};
    return entry;
}
CatalogEntry effect(const char* id,const char* kind,const char* behaviorId){
    CatalogEntry entry;entry.id=id;entry.type="Effect";
    entry.fields={{"kind",picojson::value(kind)},
                  {"behavior",picojson::value(behaviorId)}};
    return entry;
}
CatalogEntry ability(const char* id,const char* effectId){
    CatalogEntry entry;entry.id=id;entry.type="Ability";
    entry.fields={{"target",picojson::value("Self")},
        {"effect",picojson::value(effectId)},
        {"cooldown_ticks",picojson::value(10.0)},
        {"cast_range",picojson::value(0.0)},
        {"energy_cost",picojson::value(0.0)},
        {"mineral_cost",picojson::value(0.0)}};
    return entry;
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
Unit* find(RTSSkirmish& g,int id){
    for(auto& u:g.units)if(u.id==id)return &u;
    return nullptr;
}
void near(float a,float b){assert(std::fabs(a-b)<.001f);}
}
int main(){
    RTSProject project=RTSProject::defaults();
    project.metadata["enemy_ai_enabled"]=picojson::value(false);
    project.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    assert(project.catalog.add(behavior("guarded")));
    assert(project.catalog.add(effect("grant_guard","ApplyBehavior","guarded")));
    assert(project.catalog.add(effect("dispel_guard","RemoveBehavior","guarded")));
    CatalogEntry combo;
    combo.id="double_guard";combo.type="Effect";
    combo.fields={{"kind",picojson::value("Sequence")},
        {"children",picojson::value(picojson::array{
            picojson::value("grant_guard"),picojson::value("grant_guard")})}};
    assert(project.catalog.add(combo));
    assert(project.catalog.add(ability("guard_stance","grant_guard")));
    assert(project.catalog.setField("ranger","abilities",picojson::value(
        picojson::array{picojson::value("guard_stance")})));
    project.validate();
    const auto decoded=RTSProject::fromJson(project.toJson());
    assert(compileBehavior(decoded.catalog,"guarded").maxStacks==3);
    const auto ops=compileEffect(decoded.catalog,"double_guard");
    assert(ops.size()==2 && ops[0].kind==EffectOperationKind::ApplyBehavior &&
           ops[1].behaviorId=="guarded");
    assert(compileEffect(decoded.catalog,"dispel_guard")[0].kind==
           EffectOperationKind::RemoveBehavior);

    RTSSkirmish game;game.loadProject(project);
    int id=-1;
    for(const auto& u:game.units)if(u.catalogId=="ranger" && u.ownerId==0){
        id=u.id;break;
    }
    assert(id>0);
    Unit* unit=find(game,id);assert(unit);
    near(unit->behaviorArmorBonus,0.f);
    near(unit->behaviorDamageBonus,0.f);
    near(unit->behaviorSpeedFactor,1.f);
    assert(game.applyEffectForTrigger("double_guard",id));
    assert(game.activeBehaviorCount()==1);
    assert(game.unitBehaviorStacks(id,"guarded")==2);
    assert(game.unitBehaviorTicksRemaining(id,"guarded")==6);
    near(unit->behaviorArmorBonus,8.f);
    near(unit->behaviorDamageBonus,4.f);
    near(unit->behaviorSpeedFactor,.5f);
    assert(game.applyEffectForTrigger("grant_guard",id));
    assert(game.unitBehaviorStacks(id,"guarded")==3);
    near(unit->behaviorArmorBonus,12.f);
    near(unit->behaviorSpeedFactor,.25f);
    assert(game.applyEffectForTrigger("grant_guard",id));
    assert(game.unitBehaviorStacks(id,"guarded")==3); // capped stack count
    const float originalHp=unit->currentHp;
    unit->takeDamage(10.f,"test hit");
    near(unit->currentHp,originalHp-1.f); // effective armor changes combat
    assert(game.applyEffectForTrigger("dispel_guard",id));
    assert(game.unitBehaviorStacks(id,"guarded")==0);
    near(unit->behaviorArmorBonus,0.f);
    near(unit->behaviorDamageBonus,0.f);
    near(unit->behaviorSpeedFactor,1.f);
    assert(game.applyEffectForTrigger("grant_guard",id));
    for(int t=1;t<=5;++t)game.update(1.f/60.f);
    assert(game.unitBehaviorStacks(id,"guarded")==1);
    game.update(1.f/60.f); // expiry at exact tick 6
    assert(game.unitBehaviorStacks(id,"guarded")==0);
    near(unit->navAgent.maxSpeed,unit->data.move_speed);

    // Reapplication extends duration only when refresh_duration is true.
    game.reset();
    assert(game.applyEffectForTrigger("grant_guard",id));
    game.update(1.f/60.f);game.update(1.f/60.f);
    assert(game.applyEffectForTrigger("grant_guard",id));
    assert(game.unitBehaviorStacks(id,"guarded")==2);
    assert(game.unitBehaviorTicksRemaining(id,"guarded")==6);
    for(int t=3;t<=6;++t)game.update(1.f/60.f);
    assert(game.unitBehaviorStacks(id,"guarded")==2);
    game.update(1.f/60.f);game.update(1.f/60.f);
    assert(game.unitBehaviorStacks(id,"guarded")==0);

    RTSProject rigid=project;
    assert(rigid.catalog.setField("guarded","refresh_duration",picojson::value(false)));
    assert(rigid.catalog.setField("guarded","dispellable",picojson::value(false)));
    rigid.validate();
    RTSSkirmish stubborn;stubborn.loadProject(rigid);
    assert(stubborn.applyEffectForTrigger("grant_guard",id));
    for(int t=0;t<2;++t)stubborn.update(1.f/60.f);
    assert(stubborn.applyEffectForTrigger("grant_guard",id));
    assert(stubborn.unitBehaviorTicksRemaining(id,"guarded")==4);
    assert(stubborn.applyEffectForTrigger("dispel_guard",id));
    assert(stubborn.unitBehaviorStacks(id,"guarded")==2);
    for(int t=0;t<4;++t)stubborn.update(1.f/60.f);
    assert(stubborn.unitBehaviorStacks(id,"guarded")==0);

    // Cast Ability to apply the same Behavior via authoritative input,
    // recording a valid replay rather than mutating simulation behind it.
    RTSSkirmish replay;replay.loadProject(project);
    for(auto& u:replay.units)u.isSelected=(u.id==id);
    std::ofstream fingerprint("behavior_fingerprint.txt",
                               std::ios::trunc|std::ios::binary);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<replay.stateDigest()<<"\n";
    assert(replay.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,
                                     "guard_stance"));
    for(int t=1;t<=36;++t){
        if(t==2)
            assert(replay.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,
                                             "guard_stance")); // rejected cooldown
        if(t==12)
            assert(replay.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,
                                             "guard_stance"));
        replay.update(1.f/60.f);
        if(t==1 || t==2 || t==6 || t==7 || t==12 || t==18 ||
           t%12==0)fingerprint<<t<<" "<<replay.stateDigest()<<"\n";
    }
    fingerprint.flush();assert(fingerprint.good());
    assert(replay.activeBehaviorCount()==0);
    const auto outcomes=replay.takeCommandOutcomes();
    assert(outcomes.size()==3 && outcomes[0].accepted &&
           !outcomes[1].accepted && outcomes[2].accepted);
    std::string reason;
    const auto tape=replay.replayTape();
    assert(verifyRTSReplay(tape,&reason));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&reason));
    auto changed=tape;
    changed.commands[2].catalog="dispel_guard"; // not an Ability ID
    assert(!verifyRTSReplay(changed,&reason));

    // Typed dependency rewrites: Behavior rename must also rewrite the
    // Effects referencing it and reject deletion until no inbound refs.
    std::string error;
    assert(!project.removeCatalogObject("guarded",&error));
    assert(error.find("behavior")!=std::string::npos);
    assert(project.renameCatalogObject("guarded","guarded_v2"));
    assert(compileEffect(project.catalog,"grant_guard")[0].behaviorId=="guarded_v2");
    assert(project.renameCatalogObject("grant_guard","new_guard_effect"));
    assert(compileAbility(project.catalog,"guard_stance").effect=="new_guard_effect");

    for(const auto& [field,bad]:{
        std::pair<const char*,double>{"duration_ticks",0.},
        {"duration_ticks",36001.},{"max_stacks",9.},
        {"max_stacks",1.5},{"armor_bonus",51.},
        {"damage_bonus",-101.},{"speed_multiplier",0.}
    }){
        auto invalidProject=project;
        assert(invalidProject.catalog.setField("guarded_v2",field,picojson::value(bad)));
        assert(invalid(invalidProject));
    }
    auto bad=project;
    assert(bad.catalog.setField("guarded_v2","dispellable",picojson::value("no")));
    assert(invalid(bad));
    bad=project;
    assert(bad.catalog.setField("new_guard_effect","behavior",picojson::value("missing")));
    assert(invalid(bad));
    // A large number of live modifier keys has a hard global limit.
    RTSSkirmish bounded;bounded.loadProject(project);
    int admitted=0;
    for(int i=0;i<256;++i){
        assert(bounded.spawnForTrigger("ranger",0,{-13.f,0.f,-4.5f}));
        int target=bounded.units.back().id;
        assert(bounded.applyEffectForTrigger("new_guard_effect",target));
        ++admitted;
    }
    assert(admitted==256 && bounded.activeBehaviorCount()==256);
    assert(bounded.spawnForTrigger("ranger",0,{-13.f,0.f,-4.5f}));
    const int extra=bounded.units.back().id;
    const float hpBefore=bounded.units.back().currentHp;
    assert(!bounded.applyEffectForTrigger("new_guard_effect",extra));
    assert(bounded.activeBehaviorCount()==256);
    near(bounded.units.back().currentHp,hpBefore);
    bounded.reset();
    assert(bounded.activeBehaviorCount()==0);
    std::cout<<"behaviors: stacks, combat stats, movement, expiry, dispels, authoring refs, replay passed\n";
}
