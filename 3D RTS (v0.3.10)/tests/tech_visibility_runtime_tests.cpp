#include "RTSSkirmish.hpp"
#include "RTSHUDLayout.hpp"
#include <algorithm>
#include <cassert>
#if defined(NDEBUG)
#error "Tech visibility tests must run Release assertions"
#endif
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
CatalogEntry upgrade(const char* id,const char* name,const char* show="",
                     const char* use=""){
    CatalogEntry e;e.id=id;e.type="Upgrade";
    e.fields={{"name",picojson::value(name)},
              {"max_level",picojson::value(1.)},
              {"mineral_cost",picojson::value(12.)},
              {"research_ticks",picojson::value(2.)},
              {"show_requirement",picojson::value(show)},
              {"requirement",picojson::value(use)}};
    return e;
}
void addValidator(RTSProject& p,const char* id,const char* upgradeId,
                  const char* subject="Caster"){
    CatalogEntry e;e.id=id;e.type="Validator";
    e.fields={{"kind",picojson::value("UpgradeLevelAtLeast")},
              {"subject",picojson::value(subject)},
              {"upgrade",picojson::value(upgradeId)},
              {"value",picojson::value(1.)}};
    assert(p.catalog.add(e));
}
void addPredicate(RTSProject& p,const char* id,const char* validator){
    CatalogEntry e;e.id=id;e.type="Requirement";
    e.fields={{"kind",picojson::value("Predicate")},
              {"validator",picojson::value(validator)}};
    assert(p.catalog.add(e));
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
void selectOnly(RTSSkirmish& g,int id){
    for(auto& u:g.units)u.isSelected=u.isAlive() && u.ownerId==0 && u.id==id;
}
void update(RTSSkirmish& g,int ticks=1){
    for(int i=0;i<ticks;++i)g.update(1.f/60.f);
}
bool contains(const std::vector<std::string>& a,const std::string& id){
    return std::find(a.begin(),a.end(),id)!=a.end();
}
RTSProject fixture(){
    RTSProject p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.);
    assert(p.catalog.add(upgrade("open_tech","Starter Tech","r_not_open")));
    assert(p.catalog.add(upgrade("gate_tech","Gateway Tech")));
    assert(p.catalog.add(upgrade("advanced_tech","Advanced Tech",
                                 "r_open","r_gate")));
    addValidator(p,"v_open","open_tech");
    addValidator(p,"v_gate","gate_tech");
    addValidator(p,"v_advanced","advanced_tech");
    addPredicate(p,"r_open","v_open");
    addPredicate(p,"r_gate","v_gate");
    addPredicate(p,"r_advanced","v_advanced");
    CatalogEntry notOpen;notOpen.id="r_not_open";notOpen.type="Requirement";
    notOpen.fields={{"kind",picojson::value("Not")},
                    {"children",picojson::value(picojson::array{
                        picojson::value("r_open")})}};
    assert(p.catalog.add(notOpen));
    assert(p.catalog.setField("command_center","researchable_upgrades",
        picojson::value(picojson::array{picojson::value("open_tech"),
                                       picojson::value("gate_tech"),
                                       picojson::value("advanced_tech")})));
    assert(p.catalog.setField("worker","show_requirement",
                              picojson::value("r_open")));
    assert(p.catalog.setField("worker","requirement",
                              picojson::value("r_gate")));
    CatalogEntry heal;heal.id="tech_repair";heal.type="Effect";
    heal.fields={{"kind",picojson::value("Heal")},
                 {"amount",picojson::value(5.)}};
    assert(p.catalog.add(heal));
    CatalogEntry ability;ability.id="tech_pulse";ability.type="Ability";
    ability.fields={{"effect",picojson::value("tech_repair")},
                    {"target",picojson::value("Self")},
                    {"show_requirement",picojson::value("r_gate")},
                    {"requirement",picojson::value("r_advanced")},
                    {"cooldown_ticks",picojson::value(4.)}};
    assert(p.catalog.add(ability));
    assert(p.catalog.setField("ranger","abilities",picojson::value(
        picojson::array{picojson::value("tech_pulse")})));
    p.validate();return p;
}
}
int main(){
    RTSProject p=fixture();
    const auto definition=compileUpgrade(p.catalog,"advanced_tech");
    assert(definition.showRequirementId=="r_open" &&
           definition.requirementId=="r_gate");
    const auto pulse=compileAbility(p.catalog,"tech_pulse");
    assert(pulse.showRequirementId=="r_gate" &&
           pulse.requirementId=="r_advanced");
    assert(RTSProject::fromJson(p.toJson()).toJson().serialize()==
           p.toJson().serialize());

    RTSSkirmish game;game.loadProject(p);
    const int base=game.playerBaseId();
    int ranger=-1;
    for(const auto& u:game.units)
        if(u.ownerId==0 && u.isAlive() && u.catalogId=="ranger"){
            ranger=u.id;break;
        }
    assert(base>0 && ranger>0);
    selectOnly(game,base);
    const RTSHUDLayout hud(1280,720);
    // The hidden Unit and hidden tech do NOT occupy a production page slot.
    assert(game.trainableUnits(base).empty());
    assert(game.researchableUpgrades(base)==
           (std::vector<std::string>{"open_tech","gate_tech"}));
    assert(game.selectedResearchableId(0)=="open_tech");
    assert(game.selectedResearchableId(1)=="gate_tech");
    assert(game.selectedResearchableId(2).empty());
    assert(game.trainingPageCount()==1);
    auto cards=hud.buttons(game);
    assert(cards[3].enabled && cards[4].enabled);
    assert(cards[5].title=="EMPTY" && !cards[5].enabled);
    assert(game.visibleAbilities(ranger).empty());
    assert(!game.canPrepareAbility(ranger,"tech_pulse"));

    std::ofstream fingerprint("tech_visibility_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<game.stateDigest()<<"\n";
    const int initialMinerals=game.playerMinerals;
    assert(game.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,false,
                                   "advanced_tech"));
    update(game);
    assert(game.takeCommandOutcomes().back().accepted==false);
    assert(game.playerMinerals==initialMinerals);
    assert(game.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,false,
                                   "open_tech"));
    update(game,3);
    assert(game.upgradeLevel(0,"open_tech")==1);
    assert(!contains(game.researchableUpgrades(base),"open_tech"));
    assert(contains(game.researchableUpgrades(base),"advanced_tech"));
    assert(game.trainableUnits(base)==std::vector<std::string>{"worker"});
    assert(game.selectedTrainableId(0)=="worker");
    assert(game.selectedResearchableId(1)=="gate_tech");
    assert(game.selectedResearchableId(2)=="advanced_tech");
    assert(!game.canQueueSelectedTrainingSlot(0)); // visible but Use locked
    assert(!game.canResearchSelectedSlot(2));
    assert(game.visibleAbilities(ranger).empty()); // still hidden
    cards=hud.buttons(game);
    assert(cards[3].title=="WORKER" && !cards[3].enabled);
    assert(cards[5].title!="EMPTY" && !cards[5].enabled);
    fingerprint<<"4 "<<game.stateDigest()<<"\n";

    assert(game.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,false,
                                   "advanced_tech"));
    update(game);
    assert(!game.takeCommandOutcomes().back().accepted);
    assert(game.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,false,
                                   "gate_tech"));
    update(game,3);
    assert(game.upgradeLevel(0,"gate_tech")==1);
    assert(game.canQueueSelectedTrainingSlot(0)); // Use unlocked
    assert(game.canResearchSelectedSlot(2));      // Use unlocked
    assert(game.visibleAbilities(ranger)==
           std::vector<std::string>{"tech_pulse"});
    selectOnly(game,ranger);
    assert(game.selectedAbilityId(0)=="tech_pulse");
    assert(!game.canPrepareAbility(ranger,"tech_pulse")); // shown, Use locked
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,
                                   "tech_pulse"));
    update(game);
    assert(!game.takeCommandOutcomes().back().accepted);
    selectOnly(game,base);
    assert(game.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,false,
                                   "advanced_tech"));
    update(game,3);
    assert(game.upgradeLevel(0,"advanced_tech")==1);
    assert(!contains(game.researchableUpgrades(base),"open_tech"));
    selectOnly(game,ranger);
    assert(game.canPrepareAbility(ranger,"tech_pulse"));
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,false,
                                   "tech_pulse"));
    update(game);
    assert(game.takeCommandOutcomes().back().accepted);
    update(game,62);
    fingerprint<<game.simulationTick()<<" "<<game.stateDigest()<<"\n";
    fingerprint.flush();
    assert(fingerprint.good());
    std::string diagnostic;
    const auto tape=game.replayTape();
    assert(tape.commands.size()==7);
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),
                           &diagnostic));
    auto changed=tape;
    changed.commands[1].catalog="advanced_tech";
    assert(!verifyRTSReplay(changed,&diagnostic));

    // Target-scoped Show graphs can never describe a producer or command
    // visibility, and invalid authoring must be caught by project.validate.
    addValidator(p,"v_target","open_tech","Target");
    addPredicate(p,"r_target","v_target");
    for(const char* id:{"worker","tech_pulse","advanced_tech"}){
        auto invalidProject=p;
        assert(invalidProject.catalog.setField(id,"show_requirement",
                                               picojson::value("r_target")));
        assert(invalid(invalidProject));
    }
    auto bad=p;
    assert(bad.catalog.setField("advanced_tech","show_requirement",
                                picojson::value("not_a_requirement")));
    assert(invalid(bad));
    bad=p;
    assert(bad.catalog.setField("worker","show_requirement",
                                picojson::value(3.)));
    assert(invalid(bad));

    assert(p.catalogReferences("r_open").size()>=2);
    assert(p.renameCatalogObject("r_open","r_open_renamed"));
    assert(compileUpgrade(p.catalog,"advanced_tech").showRequirementId==
           "r_open_renamed");
    assert(p.catalog.resolvedFields("worker").at("show_requirement").
           get<std::string>()=="r_open_renamed");
    assert(!p.removeCatalogObject("r_open_renamed"));
    std::cout<<"Show/Use visibility: hidden vs locked Unit, Ability, Upgrade; "
                "tech unlocks, typed refs and replay parity passed\n";
}
