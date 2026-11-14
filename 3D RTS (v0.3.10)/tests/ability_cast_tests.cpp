#include "RTSSkirmish.hpp"
#include "RTSCommandStream.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Ability gameplay CTest requires live assertions in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
CatalogEntry ability(const char* id,const char* effect,const char* target,
                     double cooldown,double range,double cost){
    CatalogEntry result;
    result.type="Ability";result.id=id;
    result.fields={{"name",picojson::value(std::string(id))},
                   {"effect",picojson::value(effect)},
                   {"target",picojson::value(target)},
                   {"cooldown_ticks",picojson::value(cooldown)},
                   {"cast_range",picojson::value(range)},
                   {"mineral_cost",picojson::value(cost)}};
    return result;
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
float hp(const RTSSkirmish& game,int id){
    for(const auto& u:game.units)if(u.id==id)return u.currentHp;
    return -1.f;
}
}
int main(){
    RTSProject project=RTSProject::defaults();
    project.metadata["enemy_ai_enabled"]=picojson::value(false);
    project.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    assert(project.catalog.add(ability("self_repair","restorative_pulse","Self",60,0,8)));
    assert(project.catalog.setField("self_repair","energy_cost",picojson::value(25.0)));
    assert(project.catalog.setField("ranger","max_energy",picojson::value(100.0)));
    assert(project.catalog.setField("ranger","energy_regen",picojson::value(60.0)));
    assert(project.catalog.add(ability("energy_bolt","kinetic_pulse","Enemy",120,12,10)));
    assert(project.catalog.setField("energy_bolt","energy_cost",picojson::value(40.0)));
    assert(project.catalog.setField("ranger","abilities",picojson::value(picojson::array{
        picojson::value("self_repair"),picojson::value("energy_bolt")})));
    assert(project.catalog.cloneDerived("rift_raider","dummy_target"));
    assert(project.catalog.setField("dummy_target","name",picojson::value("Training Dummy")));
    assert(project.catalog.setField("dummy_target","damage",picojson::value(0.0)));
    assert(project.catalog.setField("dummy_target","aggro_range",picojson::value(0.0)));
    assert(project.catalog.setField("dummy_target","max_hp",picojson::value(300.0)));
    ProjectPlacement target;
    target.id="ability_target";target.kind="Unit";target.catalogId="dummy_target";
    target.name="Target Dummy";target.owner=1;target.position={-4.f,0.f,-4.5f};
    project.placements.push_back(target);
    project.validate();
    const auto saved=RTSProject::fromJson(project.toJson());
    assert(unitAbilityIds(saved.catalog,"ranger").size()==2);
    assert(compileAbility(saved.catalog,"self_repair").target==AbilityTarget::Self);
    assert(compileAbility(saved.catalog,"energy_bolt").target==AbilityTarget::Enemy);
    RTSSkirmish game;game.loadProject(saved);
    int caster=-1,enemy=-1;
    for(auto& u:game.units){
        if(u.catalogId=="ranger" && caster<0){caster=u.id;u.isSelected=true;}
        if(u.catalogId=="dummy_target")enemy=u.id;
    }
    assert(caster>0 && enemy>0);
    assert(game.selectedAbilityId(0)=="self_repair");
    assert(game.selectedAbilityId(1)=="energy_bolt");
    assert(game.selectedAbilityId(2).empty());
    assert(game.canPrepareAbility(caster,"self_repair"));
    assert(game.canCastAbility(caster,"self_repair",-1));
    assert(!game.canCastAbility(caster,"energy_bolt",-1));
    assert(!game.canCastAbility(caster,"energy_bolt",caster));
    assert(game.canCastAbility(caster,"energy_bolt",enemy));
    const int initialMinerals=game.playerMinerals;
    const float initialHp=hp(game,enemy);
    std::ofstream out("ability_cast_fingerprint.txt",std::ios::trunc|std::ios::binary);
    assert(out.is_open());
    out<<"0 "<<game.stateDigest()<<"\n";
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,"self_repair"));
    game.update(1.f/60.f);
    assert(game.playerMinerals==initialMinerals-8);
    for(const auto& u:game.units)if(u.id==caster)
        assert(u.currentEnergy==76.f);
    assert(game.abilityCooldownRemaining(caster,"self_repair")==60);
    assert(!game.canPrepareAbility(caster,"self_repair"));
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,"self_repair"));
    game.update(1.f/60.f);
    const auto rejected=game.takeCommandOutcomes();
    assert(rejected.size()==2 && rejected[0].accepted && !rejected[1].accepted);
    assert(game.playerMinerals==initialMinerals-8);
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},enemy,false,"energy_bolt"));
    game.update(1.f/60.f);
    assert(game.playerMinerals==initialMinerals-18);
    for(const auto& u:game.units)if(u.id==caster)
        assert(u.currentEnergy==38.f);
    assert(std::fabs(hp(game,enemy)-(initialHp-12.f))<.02f);
    assert(game.abilityCooldownRemaining(caster,"energy_bolt")==120);
    for(int tick=4;tick<=90;++tick){
        if(tick==62){
            assert(game.abilityCooldownRemaining(caster,"self_repair")==0);
            assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,"self_repair"));
        }
        game.update(1.f/60.f);
        if(tick%15==0)out<<tick<<" "<<game.stateDigest()<<"\n";
    }
    assert(game.playerMinerals==initialMinerals-26);
    for(const auto& u:game.units)if(u.id==caster)
        assert(u.currentEnergy<=u.data.max_energy && u.currentEnergy>=0.f);
    out.flush();assert(out.good());
    const auto tape=game.replayTape();
    assert(tape.commands.size()==4);
    for(const auto& c:tape.commands)
        assert(c.kind==RTSCommandKind::CastAbility);
    assert(tape.commands[2].targetId==enemy);
    assert(commandFromJson(commandToJson(tape.commands[2])).catalog=="energy_bolt");
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson())));
    auto changed=tape;
    changed.commands[2].catalog="self_repair";
    assert(!verifyRTSReplay(changed,&diagnostic));
    assert(diagnostic.find("diverged")!=std::string::npos);

    std::string reason;
    assert(!project.removeCatalogObject("energy_bolt",&reason));
    assert(reason.find("abilities")!=std::string::npos);
    assert(project.renameCatalogObject("energy_bolt","energy_bolt_v2"));
    assert(unitAbilityIds(project.catalog,"ranger")[1]=="energy_bolt_v2");
    assert(!project.removeCatalogObject("kinetic_pulse",&reason));
    assert(reason.find("effect")!=std::string::npos);
    assert(project.renameCatalogObject("kinetic_pulse","kinetic_pulse_v2"));
    assert(compileAbility(project.catalog,"energy_bolt_v2").effect=="kinetic_pulse_v2");

    RTSProject bad=project;
    assert(bad.catalog.setField("energy_bolt_v2","cast_range",picojson::value(129.0)));
    assert(invalid(bad));
    bad=project;
    assert(bad.catalog.setField("energy_bolt_v2","cooldown_ticks",picojson::value(1.5)));
    assert(invalid(bad));
    bad=project;
    assert(bad.catalog.setField("energy_bolt_v2","energy_cost",picojson::value(-2.0)));
    assert(invalid(bad));
    bad=project;
    assert(bad.catalog.setField("ranger","max_energy",picojson::value(-1.0)));
    assert(invalid(bad));
    bad=project;
    assert(bad.catalog.setField("energy_bolt_v2","target",picojson::value("Terrain")));
    assert(invalid(bad));
    bad=project;
    assert(bad.catalog.setField("energy_bolt_v2","effect",picojson::value("bogus")));
    assert(invalid(bad));
    bad=project;
    assert(bad.catalog.setField("ranger","abilities",picojson::value(picojson::array{
        picojson::value("self_repair"),picojson::value("self_repair")})));
    assert(invalid(bad));
    game.reset();
    assert(game.abilityCooldownRemaining(caster,"self_repair")==0);
    assert(game.playerMinerals==initialMinerals);
    std::cout<<"ability casts: authored catalog, target policy, cooldowns, replay and IDs passed\n";
}
