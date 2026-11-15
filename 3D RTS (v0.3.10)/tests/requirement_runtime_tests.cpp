#include "RTSSkirmish.hpp"
#include "RTSRequirements.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Requirement runtime tests require active assertions in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
CatalogEntry validator(const char* id,const char* kind,const char* subject,
                       double value,const char* unit=""){
    CatalogEntry e;e.id=id;e.type="Validator";
    e.fields={{"kind",picojson::value(kind)},{"subject",picojson::value(subject)},
              {"value",picojson::value(value)},{"unit",picojson::value(unit)}};
    return e;
}
CatalogEntry predicate(const char* id,const char* reference){
    CatalogEntry e;e.id=id;e.type="Requirement";
    e.fields={{"kind",picojson::value("Predicate")},
              {"validator",picojson::value(reference)}};
    return e;
}
CatalogEntry group(const char* id,const char* kind,
                   std::initializer_list<const char*> children){
    picojson::array ids;
    for(const char* child:children)ids.emplace_back(child);
    CatalogEntry e;e.id=id;e.type="Requirement";
    e.fields={{"kind",picojson::value(kind)},
              {"children",picojson::value(ids)}};
    return e;
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
int locate(const RTSSkirmish& g,const char* name){
    for(const auto& u:g.units)if(u.instanceName==name)return u.id;
    return -1;
}
RTSProject fixture(){
    RTSProject p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.);
    // Baseline Ranger defaults to no energy; grant it a deterministic
    // 100-point pool to exercise an actual caster EnergyAtLeast gate.
    assert(p.catalog.setField("ranger","max_energy",picojson::value(100.)));
    assert(p.catalog.add(validator("target_hurt","HealthBelowPercent","Target",50.)));
    assert(p.catalog.add(validator("caster_energy","EnergyAtLeast","Caster",1.)));
    assert(p.catalog.add(validator("target_structure","IsStructure","Target",1.)));
    assert(p.catalog.add(validator("owner_rangers","OwnerUnitCountAtLeast",
                                   "Caster",3.,"ranger")));
    assert(p.catalog.add(predicate("r_hurt","target_hurt")));
    assert(p.catalog.add(predicate("r_energy","caster_energy")));
    assert(p.catalog.add(predicate("r_structure","target_structure")));
    assert(p.catalog.add(predicate("r_owner","owner_rangers")));
    assert(p.catalog.add(group("r_not_structure","Not",{"r_structure"})));
    assert(p.catalog.add(group("r_any","Any",{"r_hurt","r_structure"})));
    assert(p.catalog.add(group("r_cast","All",{"r_energy","r_any",
                                               "r_not_structure"})));
    assert(p.catalog.add(group("r_train","All",{"r_owner"})));
    CatalogEntry effect;effect.type="Effect";effect.id="heavy_hit";
    effect.fields={{"kind",picojson::value("Damage")},
                   {"amount",picojson::value(130.)}};
    assert(p.catalog.add(effect));
    CatalogEntry poke;poke.type="Ability";poke.id="poke";
    poke.fields={{"effect",picojson::value("heavy_hit")},
                 {"target",picojson::value("Enemy")},
                 {"cast_range",picojson::value(11.)},
                 {"cooldown_ticks",picojson::value(5.)}};
    assert(p.catalog.add(poke));
    CatalogEntry finish;finish.type="Ability";finish.id="finish";
    finish.fields={{"effect",picojson::value("kinetic_pulse")},
                   {"target",picojson::value("Enemy")},
                   {"cast_range",picojson::value(11.)},
                   {"cooldown_ticks",picojson::value(10.)},
                   {"requirement",picojson::value("r_cast")}};
    assert(p.catalog.add(finish));
    assert(p.catalog.setField("ranger","abilities",picojson::value(
        picojson::array{picojson::value("poke"),picojson::value("finish")})));
    // Worker training, not existing placed Workers, is gated.
    assert(p.catalog.setField("worker","requirement",
                              picojson::value("r_train")));
    assert(p.catalog.cloneDerived("rift_raider","req_dummy"));
    for(const char* name:{"damage","attack_range","aggro_range","move_speed"})
        assert(p.catalog.setField("req_dummy",name,picojson::value(0.)));
    assert(p.catalog.setField("req_dummy","max_hp",picojson::value(200.)));
    assert(p.catalog.setField("req_dummy","armor",picojson::value(0.)));
    ProjectPlacement enemy;enemy.id="requirement_target";enemy.kind="Unit";
    enemy.catalogId="req_dummy";enemy.name="Requirement Dummy";enemy.owner=1;
    enemy.position={-8.f,0.f,-4.5f};p.placements.push_back(enemy);
    p.validate();return p;
}
}
int main(){
    RTSProject p=fixture();
    const auto graph=compileRequirement(p.catalog,"r_cast");
    assert(graph.kind==RTSRequirementKind::All && graph.children.size()==3);
    assert(graph.children[1].kind==RTSRequirementKind::Any);
    assert(graph.children[2].kind==RTSRequirementKind::Not);
    assert(rtsRequirementUsesTarget(graph));
    assert(!rtsRequirementUsesTarget(compileRequirement(p.catalog,"r_train")));
    assert(compileAbility(p.catalog,"finish").requirementId=="r_cast");
    RTSSkirmish game;game.loadProject(RTSProject::fromJson(p.toJson()));
    int caster=-1,target=locate(game,"Requirement Dummy");
    for(auto& unit:game.units)
        if(caster<0 && unit.catalogId=="ranger" && unit.ownerId==0){
            caster=unit.id;unit.isSelected=true;
        }
    assert(caster>0 && target>0);
    assert(!game.canCastAbility(caster,"finish",target));
    assert(game.canCastAbility(caster,"poke",target));
    const float initialHp=[&](){
        for(const auto& unit:game.units)if(unit.id==target)return unit.currentHp;
        return 0.f;
    }();
    assert(initialHp==200.f);
    std::ofstream out("requirement_fingerprint.txt",
                      std::ios::binary|std::ios::trunc);
    assert(out.is_open());
    out<<"0 "<<game.stateDigest()<<"\n";
    // Invalid command is still recorded deterministically; no resources
    // or cooldown are consumed by the failed preflight.
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},
                                   target,false,"finish"));
    game.update(1.f/60.f);
    assert(game.abilityCooldownRemaining(caster,"finish")==0);
    const auto rejected=game.takeCommandOutcomes();
    assert(rejected.size()==1 && !rejected[0].accepted);
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},
                                   target,false,"poke"));
    game.update(1.f/60.f);
    assert(game.canCastAbility(caster,"finish",target));
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},
                                   target,false,"finish"));
    game.update(1.f/60.f);
    const auto responses=game.takeCommandOutcomes();
    assert(responses.size()==2 && responses[0].accepted &&
           responses[1].accepted);
    assert(game.abilityCooldownRemaining(caster,"finish")==10);
    for(int tick=4;tick<=75;++tick){
        game.update(1.f/60.f);
        if(tick%15==0)out<<tick<<" "<<game.stateDigest()<<"\n";
    }
    out.flush();assert(out.good());
    const auto tape=game.replayTape();std::string why;
    assert(tape.commands.size()==3);
    assert(verifyRTSReplay(tape,&why));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&why));
    auto tampered=tape;tampered.commands[1].catalog="finish";
    assert(!verifyRTSReplay(tampered,&why));

    // Training a Worker requires three living Rangers. Gating must happen
    // at queue admission, not completion or a UI-only disabled button.
    RTSSkirmish train;train.loadProject(p);
    const int baseId=train.playerBaseId();
    assert(baseId>0);
    for(auto& u:train.units)u.isSelected=u.id==baseId;
    assert(train.canQueueSelectedTrainingSlot(0));
    const int initialMinerals=train.playerMinerals;
    assert(train.queuePlayerCommand(RTSCommandKind::TrainWorker));
    train.update(1.f/60.f);
    assert(train.playerMinerals<initialMinerals);
    RTSProject locked=p;
    assert(locked.catalog.setField("owner_rangers","value",picojson::value(100.)));
    locked.validate();
    RTSSkirmish unavailable;unavailable.loadProject(locked);
    const int lockedBase=unavailable.playerBaseId();
    for(auto& u:unavailable.units)u.isSelected=u.id==lockedBase;
    assert(!unavailable.canQueueSelectedTrainingSlot(0));
    const int before=unavailable.playerMinerals;
    assert(unavailable.queuePlayerCommand(RTSCommandKind::TrainWorker));
    unavailable.update(1.f/60.f);
    assert(unavailable.playerMinerals==before);

    // Target-only gates cannot be used for ground-targeted Abilities or
    // production (no target exists), including when nested under Not/Any.
    auto ground=p;
    assert(ground.catalog.setField("finish","target",
                                   picojson::value("Ground")));
    assert(invalid(ground));
    auto production=p;
    assert(production.catalog.setField("worker","requirement",
                                       picojson::value("r_cast")));
    assert(invalid(production));
    auto bad=p;
    assert(bad.catalog.setField("owner_rangers","value",
                                picojson::value(2.5)));
    assert(invalid(bad));
    bad=p;
    assert(bad.catalog.setField("owner_rangers","unit",
                                picojson::value("absent_unit")));
    assert(invalid(bad));
    bad=p;
    assert(bad.catalog.setField("r_not_structure","children",
                    picojson::value(picojson::array{picojson::value("r_cast")})));
    assert(invalid(bad)); // cyclic graph
    bad=p;
    assert(bad.catalog.setField("r_cast","children",
                    picojson::value(picojson::array{})));
    assert(invalid(bad));
    bad=p;
    assert(bad.catalog.setField("r_hurt","validator",
                                picojson::value("no_validator")));
    assert(invalid(bad));
    std::cout<<"Authoritative Requirement graphs gate cast/production, survive replay and reject invalid authoring\n";
}
