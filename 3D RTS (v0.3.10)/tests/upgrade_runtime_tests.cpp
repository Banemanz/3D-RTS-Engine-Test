#include "RTSSkirmish.hpp"
#include "RTSUpgrades.hpp"
#include "RTSHUDLayout.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Upgrade tests must keep assertions enabled in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
CatalogEntry upgrade(const char* id,double cost,double duration,
                     double damage,double armor){
    CatalogEntry e;e.id=id;e.type="Upgrade";
    e.fields={{"name",picojson::value("Frontier Tactics")},
              {"max_level",picojson::value(2.)},
              {"mineral_cost",picojson::value(cost)},
              {"research_ticks",picojson::value(duration)},
              {"damage_per_level",picojson::value(damage)},
              {"armor_per_level",picojson::value(armor)},
              {"unit",picojson::value("ranger")}};
    return e;
}
RTSProject fixture(){
    RTSProject p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.);
    assert(p.catalog.add(upgrade("frontier_tactics",25.,6.,4.,2.)));
    assert(p.catalog.setField("command_center","researchable_upgrades",
        picojson::value(picojson::array{picojson::value("frontier_tactics")})));
    // Derived enemy and player structures can explicitly opt out.
    assert(p.catalog.setField("rift_bastion","researchable_upgrades",
        picojson::value(picojson::array{})));
    assert(p.catalog.setField("barracks","researchable_upgrades",
        picojson::value(picojson::array{})));
    CatalogEntry check;check.id="researched_tactics";check.type="Validator";
    check.fields={{"kind",picojson::value("UpgradeLevelAtLeast")},
                  {"subject",picojson::value("Caster")},
                  {"value",picojson::value(1.)},
                  {"upgrade",picojson::value("frontier_tactics")}};
    assert(p.catalog.add(check));
    CatalogEntry graph;graph.id="requires_tactics";graph.type="Requirement";
    graph.fields={{"kind",picojson::value("Predicate")},
                  {"validator",picojson::value("researched_tactics")}};
    assert(p.catalog.add(graph));
    assert(p.catalog.setField("worker","requirement",
                              picojson::value("requires_tactics")));
    p.validate();return p;
}
const Unit* findUnit(const RTSSkirmish& g,const char* catalog){
    for(const auto& u:g.units)
        if(u.isAlive() && u.catalogId==catalog && u.ownerId==0)return &u;
    return nullptr;
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
void step(RTSSkirmish& g,int n){
    for(int i=0;i<n;++i)g.update(1.f/60.f);
}
}
int main(){
    RTSProject p=fixture();
    const auto upgradeDefinition=compileUpgrade(p.catalog,"frontier_tactics");
    assert(upgradeDefinition.maxLevel==2 && upgradeDefinition.researchTicks==6);
    assert(upgradeDefinition.damagePerLevel==4.f);
    assert(unitResearchIds(p.catalog,"command_center").size()==1);
    assert(unitResearchIds(p.catalog,"rift_bastion").empty());
    const auto roundtrip=RTSProject::fromJson(p.toJson());
    assert(unitResearchIds(roundtrip.catalog,"command_center")[0]=="frontier_tactics");
    RTSSkirmish game;game.loadProject(roundtrip);
    const int base=game.playerBaseId();
    assert(base>0);
    for(auto& u:game.units)u.isSelected=u.id==base;
    assert(game.selectedTrainableId(0)=="worker");
    assert(game.selectedResearchableId(1)=="frontier_tactics");
    assert(game.trainingPageCount()==1);
    const RTSHUDLayout hud(1280,720);
    auto buttons=hud.buttons(game);
    assert(!buttons[3].enabled && buttons[3].title=="WORKER");
    assert(buttons[4].enabled && buttons[4].title=="FRONTI 1/2");
    // Verify the exact native bitmap-font advance fits research cards
    // in BOTH standard and compact button layouts.
    for(const auto& item:buttons){
        const bool compact=item.bounds.h<33.f;
        const float pixel=rtsCommandTitlePixelSize(item,compact);
        assert(item.title.size()*6.f*pixel<=item.bounds.w-12.f+.01f);
    }
    assert(game.upgradeLevel(0,"frontier_tactics")==0);
    assert(!game.canQueueSelectedTrainingSlot(0)); // no tech
    assert(game.canResearchSelectedSlot(1));
    const Unit* initialRanger=findUnit(game,"ranger");
    assert(initialRanger && initialRanger->upgradeDamageBonus==0.f);
    std::ofstream out("upgrade_research_fingerprint.txt",
                      std::ios::binary|std::ios::trunc);
    assert(out.is_open());
    out<<"0 "<<game.stateDigest()<<"\n";
    const int minerals=game.playerMinerals;
    assert(game.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,false,
                                   "frontier_tactics"));
    game.update(1.f/60.f);
    assert(game.playerMinerals==minerals-25);
    assert(game.pendingProduction(base)==1);
    const auto snapshot=game.stateDigest();
    const auto watch=game.researchWatch();
    assert(watch.size()==1 && watch[0].upgradeId=="frontier_tactics" &&
           watch[0].nextLevel==1 && watch[0].remainingTicks==6 &&
           watch[0].producerId==base && watch[0].progress==0.f);
    assert(game.stateDigest()==snapshot);
    assert(game.researchRemainingTicks(base)==6);
    assert(!game.canResearchSelectedSlot(1));
    assert(!game.canQueueSelectedTrainingSlot(0));
    assert(game.upgradeLevel(0,"frontier_tactics")==0);
    out<<"1 "<<game.stateDigest()<<"\n";
    step(game,5);
    assert(game.upgradeLevel(0,"frontier_tactics")==0);
    step(game,1);
    assert(game.pendingProduction(base)==0);
    assert(game.upgradeLevel(0,"frontier_tactics")==1);
    assert(game.canQueueSelectedTrainingSlot(0));
    assert(game.canResearchSelectedSlot(1));
    buttons=hud.buttons(game);
    assert(buttons[3].enabled && buttons[4].title=="FRONTI 2/2");
    const Unit* ranger=findUnit(game,"ranger");
    const Unit* worker=findUnit(game,"worker");
    assert(ranger && std::fabs(ranger->upgradeDamageBonus-4.f)<.001f);
    assert(std::fabs(ranger->upgradeArmorBonus-2.f)<.001f);
    assert(worker && worker->upgradeArmorBonus==0.f);
    out<<"7 "<<game.stateDigest()<<"\n";
    assert(game.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,false,
                                   "frontier_tactics"));
    step(game,7);
    assert(game.upgradeLevel(0,"frontier_tactics")==2);
    assert(!game.canResearchSelectedSlot(1)); // max level reached
    buttons=hud.buttons(game);
    assert(!buttons[4].enabled && buttons[4].title=="MAX FRONTI");
    assert(findUnit(game,"ranger")->upgradeDamageBonus==8.f);
    assert(findUnit(game,"ranger")->upgradeArmorBonus==4.f);
    out<<"14 "<<game.stateDigest()<<"\n";
    step(game,65);
    out<<"79 "<<game.stateDigest()<<"\n";
    out.flush();assert(out.good());
    std::string why;
    const auto tape=game.replayTape();
    assert(tape.commands.size()==2);
    assert(tape.commands[0].kind==RTSCommandKind::ResearchUpgrade);
    assert(verifyRTSReplay(tape,&why));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&why));
    auto corrupted=tape;
    corrupted.commands[1].catalog="missing_tech";
    assert(!verifyRTSReplay(corrupted,&why));

    // A matching trigger-spawned Unit also inherits already-researched tech.
    // This direct trigger helper is exercised *after* replay tape validation.
    assert(game.spawnForTrigger("ranger",0,Vec3{4.f,0.f,7.f}));
    int upgraded=0;
    for(const auto& u:game.units)if(u.catalogId=="ranger" && u.ownerId==0){
        assert(u.upgradeDamageBonus==8.f);
        assert(u.upgradeArmorBonus==4.f);
        ++upgraded;
    }
    assert(upgraded>1);
    Unit* protectedUnit=nullptr;
    for(auto& u:game.units)if(u.catalogId=="ranger" && u.ownerId==0){
        protectedUnit=&u;break;
    }
    assert(protectedUnit);
    const float beforeHp=protectedUnit->currentHp;
    protectedUnit->takeDamage(15.f,"physical-test");
    assert(std::fabs((beforeHp-protectedUnit->currentHp)-11.f)<.001f);
    game.reset();
    assert(game.upgradeLevel(0,"frontier_tactics")==0);
    assert(findUnit(game,"ranger")->upgradeDamageBonus==0.f);

    // Cancellation refunds exactly once; destroyed producers do not grant
    // their unfinished research, and both are replay-recorded commands.
    RTSSkirmish cancelled;cancelled.loadProject(p);
    const int cancelledBase=cancelled.playerBaseId();
    for(auto& u:cancelled.units)u.isSelected=u.id==cancelledBase;
    const int wallet=cancelled.playerMinerals;
    assert(cancelled.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,{},-1,
                                       false,"frontier_tactics"));
    cancelled.update(1.f/60.f);
    assert(cancelled.playerMinerals==wallet-25);
    assert(cancelled.queuePlayerCommand(RTSCommandKind::CancelTraining));
    cancelled.update(1.f/60.f);
    assert(cancelled.pendingProduction(cancelledBase)==0);
    assert(cancelled.playerMinerals==wallet);
    step(cancelled,12);
    assert(cancelled.upgradeLevel(0,"frontier_tactics")==0);
    assert(verifyRTSReplay(cancelled.replayTape(),&why));

    // Invalid catalog schema/typed refs must be rejected, including
    // nonintegral researched levels and Target-only prerequisites.
    for(const std::pair<const char*,double>& field:std::vector<
          std::pair<const char*,double>>{{"max_level",0.},
                                        {"max_level",2.5},
                                        {"research_ticks",0.},
                                        {"research_ticks",36001.},
                                        {"mineral_cost",-1.},
                                        {"damage_per_level",101.}}){
        auto bad=p;
        assert(bad.catalog.setField("frontier_tactics",field.first,
                                    picojson::value(field.second)));
        assert(invalid(bad));
    }
    auto bad=p;
    assert(bad.catalog.setField("frontier_tactics","unit",
                                picojson::value("no_unit")));
    assert(invalid(bad));
    bad=p;
    assert(bad.catalog.setField("command_center","researchable_upgrades",
        picojson::value(picojson::array{picojson::value("frontier_tactics"),
                                       picojson::value("frontier_tactics")})));
    assert(invalid(bad));
    bad=p;
    assert(bad.catalog.setField("researched_tactics","value",
                                picojson::value(1.5)));
    assert(invalid(bad));
    bad=p;
    assert(bad.catalog.setField("frontier_tactics","requirement",
                                picojson::value("requires_tactics")));
    bad.validate(); // self-locked tech is authorable but cannot be started
    RTSSkirmish locked;locked.loadProject(bad);
    for(auto& u:locked.units)u.isSelected=u.id==locked.playerBaseId();
    assert(!locked.canResearchSelectedSlot(1));
    // A level-specific table overrides the next level's price, duration
    // and incremental stat gains without changing legacy upgrade behavior.
    RTSProject stepped=fixture();
    auto levelRow=[](double cost,double ticks,double damage,double armor){
        return picojson::value(picojson::object{
            {"mineral_cost",picojson::value(cost)},
            {"research_ticks",picojson::value(ticks)},
            {"damage_bonus",picojson::value(damage)},
            {"armor_bonus",picojson::value(armor)}});
    };
    const picojson::array table{levelRow(13.,4.,2.,1.),
                                levelRow(40.,10.,7.,3.)};
    assert(stepped.catalog.setField("frontier_tactics","levels",
                                    picojson::value(table)));
    stepped.validate();
    const auto parsed=compileUpgrade(stepped.catalog,"frontier_tactics");
    assert(parsed.customLevels && parsed.levels.size()==2);
    assert(parsed.levels[0].mineralCost==13 &&
           parsed.levels[1].researchTicks==10 &&
           parsed.levels[1].damageBonus==7.f);
    const auto persisted=RTSProject::fromJson(stepped.toJson());
    assert(compileUpgrade(persisted.catalog,"frontier_tactics").customLevels);
    RTSSkirmish steppedGame;steppedGame.loadProject(persisted);
    const int steppedBase=steppedGame.playerBaseId();
    for(auto& u:steppedGame.units)u.isSelected=u.id==steppedBase;
    const int startingMinerals=steppedGame.playerMinerals;
    assert(steppedGame.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,
        {},-1,false,"frontier_tactics"));
    steppedGame.update(1.f/60.f);
    assert(steppedGame.playerMinerals==startingMinerals-13);
    assert(steppedGame.researchRemainingTicks(steppedBase)==4);
    step(steppedGame,4);
    assert(steppedGame.upgradeLevel(0,"frontier_tactics")==1);
    assert(findUnit(steppedGame,"ranger")->upgradeDamageBonus==2.f);
    assert(findUnit(steppedGame,"ranger")->upgradeArmorBonus==1.f);
    out<<"table-1 "<<steppedGame.stateDigest()<<"\n";
    // Cancel second-level research and refund the exact higher price.
    assert(steppedGame.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,
        {},-1,false,"frontier_tactics"));
    steppedGame.update(1.f/60.f);
    assert(steppedGame.playerMinerals==startingMinerals-53);
    assert(steppedGame.researchRemainingTicks(steppedBase)==10);
    assert(steppedGame.queuePlayerCommand(RTSCommandKind::CancelTraining));
    steppedGame.update(1.f/60.f);
    assert(steppedGame.playerMinerals==startingMinerals-13);
    assert(steppedGame.upgradeLevel(0,"frontier_tactics")==1);
    assert(steppedGame.queuePlayerCommand(RTSCommandKind::ResearchUpgrade,
        {},-1,false,"frontier_tactics"));
    steppedGame.update(1.f/60.f);
    step(steppedGame,10);
    assert(steppedGame.upgradeLevel(0,"frontier_tactics")==2);
    assert(findUnit(steppedGame,"ranger")->upgradeDamageBonus==9.f);
    assert(findUnit(steppedGame,"ranger")->upgradeArmorBonus==4.f);
    assert(steppedGame.playerMinerals==startingMinerals-53);
    out<<"table-2 "<<steppedGame.stateDigest()<<"\n";
    assert(verifyRTSReplay(steppedGame.replayTape(),&why));
    auto invalidRows=stepped;
    assert(invalidRows.catalog.setField("frontier_tactics","levels",
                                       picojson::value(picojson::array{table[0]})));
    assert(invalid(invalidRows)); // row count mismatch
    invalidRows=stepped;
    auto invalidTable=table;
    invalidTable[1]=levelRow(40.,0.,7.,3.);
    assert(invalidRows.catalog.setField("frontier_tactics","levels",
                                       picojson::value(invalidTable)));
    assert(invalid(invalidRows)); // zero duration
    invalidRows=stepped;
    invalidTable[1]=levelRow(-1.,10.,7.,3.);
    assert(invalidRows.catalog.setField("frontier_tactics","levels",
                                       picojson::value(invalidTable)));
    assert(invalid(invalidRows)); // negative cost
    invalidRows=stepped;
    invalidTable[1]=levelRow(40.,10.,101.,3.);
    assert(invalidRows.catalog.setField("frontier_tactics","levels",
                                       picojson::value(invalidTable)));
    assert(invalid(invalidRows)); // out-of-range stat
    out.flush();assert(out.good());
    std::cout<<"Owner research, per-level cost/time/stats, cancellation and replay passed\n";
}
