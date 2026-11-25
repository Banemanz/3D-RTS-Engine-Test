#include "RTSSkirmish.hpp"
#include "RTSAIPlanner.hpp"
#include "RTSAIResearchPolicy.hpp"
#include "EditorSession.hpp"
#include <algorithm>
#include <cassert>
#if defined(NDEBUG)
#error "AI research tests require assertions in optimized builds"
#endif
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void step(RTSSkirmish& g,int n){
    for(int i=0;i<n;++i)g.update(1.f/60.f);
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
int enemyBase(const RTSSkirmish& g){
    for(const auto& unit:g.units)
        if(unit.ownerId==1 && unit.isStructure &&
           unit.catalogId=="rift_bastion")return unit.id;
    return -1;
}
int aiResearchCommands(const RTSReplayTape& tape,const char* id){
    int n=0;
    for(const auto& c:tape.commands)if(c.source==1 &&
        c.kind==RTSCommandKind::ResearchUpgrade && c.catalog==id)++n;
    return n;
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("Expected AI Academy project path");
    RTSProject project;project.loadFile(argv[1]);
    const std::string snapshot=project.toJson().serialize();
    const auto settings=rtsAIResearchSettings(project);
    assert(settings.enabled && settings.startingMinerals==75);
    assert(settings.intervalTicks==30 && settings.upgradePriorities.size()==2);
    assert(settings.upgradePriorities[0]=="raider_plating");
    assert(settings.upgradePriorities[1]=="raider_doctrine");
    const auto candidates=std::vector<RTSAIResearchCandidate>{
        {80,"raider_doctrine",1,15},{33,"raider_doctrine",1,15},
        {17,"raider_plating",1,20}};
    auto choice=planRTSEnemyResearchOrders(candidates,settings.upgradePriorities,2);
    assert(choice.size()==1 && choice[0].catalog=="raider_plating");
    assert(choice[0].units==std::vector<int>{17});
    validateRTSCommand(choice[0]);
    auto reordered=candidates;
    std::reverse(reordered.begin(),reordered.end());
    assert(planRTSEnemyResearchOrders(reordered,settings.upgradePriorities,2)[0].units
           ==std::vector<int>{17});
    assert(planRTSEnemyResearchOrders({},settings.upgradePriorities,2).empty());
    assert(planRTSEnemyResearchOrders(candidates,settings.upgradePriorities,0).empty());
    RTSSkirmish game;game.loadProject(project);
    assert(game.enemyResearchMinerals()==75);
    const int producer=enemyBase(game);
    assert(producer>0 && game.playerBaseId()>0);
    const int playerWallet=game.playerMinerals;
    const Unit* originalRaider=nullptr;
    for(const auto& u:game.units)if(u.ownerId==1 && u.catalogId=="rift_raider"){
        originalRaider=&u;break;
    }
    assert(originalRaider && originalRaider->upgradeDamageBonus==0.f);
    step(game,1);
    assert(game.replayTape().commands.empty()); // proposed for tick 2
    step(game,1);
    assert(game.enemyResearchMinerals()==60 && game.playerMinerals==playerWallet);
    assert(game.researchRemainingTicks(producer)==4);
    assert(game.upgradeLevel(1,"raider_doctrine")==0);
    step(game,4);
    assert(game.upgradeLevel(1,"raider_doctrine")==1);
    assert(game.enemyResearchMinerals()==60);
    bool damageUpgraded=false;
    for(const auto& u:game.units)if(u.ownerId==1 && u.catalogId=="rift_raider"){
        assert(u.upgradeDamageBonus==2.f);
        damageUpgraded=true;
    }
    assert(damageUpgraded);
    step(game,26); // at 32 AI has queued second, priority-gated research
    assert(game.enemyResearchMinerals()==40);
    assert(game.researchRemainingTicks(producer)==5);
    step(game,5);
    assert(game.upgradeLevel(1,"raider_plating")==1);
    assert(game.upgradeLevel(1,"raider_doctrine")==1);
    step(game,25); // at 62 AI queues next doctrine level
    assert(game.enemyResearchMinerals()==10);
    step(game,6);
    assert(game.upgradeLevel(1,"raider_doctrine")==2);
    for(const auto& u:game.units)if(u.ownerId==1 && u.catalogId=="rift_raider"){
        assert(u.upgradeDamageBonus==5.f);
        assert(u.upgradeArmorBonus==3.f);
    }
    step(game,52);
    assert(game.simulationTick()==120 && game.enemyResearchMinerals()==10);
    assert(game.playerMinerals==playerWallet); // separate owner economy
    const auto tape=game.replayTape();
    assert(tape.version==2 && tape.totalTicks==120);
    assert(aiResearchCommands(tape,"raider_doctrine")==2);
    assert(aiResearchCommands(tape,"raider_plating")==1);
    for(const auto& cmd:tape.commands)if(cmd.source==1 &&
        cmd.kind==RTSCommandKind::ResearchUpgrade){
        assert(cmd.units==std::vector<int>{producer});
        assert(cmd.targetId==-1 && !cmd.append);
        validateRTSCommand(cmd);
    }
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&diagnostic));
    auto tampered=tape;
    for(auto& cmd:tampered.commands)if(cmd.source==1 &&
        cmd.kind==RTSCommandKind::ResearchUpgrade){
        cmd.catalog="frontier_tactics";break;
    }
    assert(!verifyRTSReplay(tampered,&diagnostic));
    std::ofstream fingerprints("ai_research_fingerprint.txt",
                                std::ios::binary|std::ios::trunc);
    assert(fingerprints.is_open());
    for(const auto& checkpoint:tape.checkpoints)
        fingerprints<<checkpoint.tick<<" "<<checkpoint.digest<<"\n";
    fingerprints.flush();assert(fingerprints.good());

    // A valid wire-shape does NOT authorize controlling the player's
    // building as source=AI: runtime admission must reject foreign owner.
    RTSSkirmish foreign;foreign.loadProject(project);
    RTSCommand injected;
    injected.tick=1;injected.source=1;
    injected.kind=RTSCommandKind::ResearchUpgrade;
    injected.units={foreign.playerBaseId()};
    injected.catalog="raider_doctrine";
    assert(foreign.queueCommand(injected));
    foreign.update(1.f/60.f);
    auto outcomes=foreign.takeCommandOutcomes();
    assert(!outcomes.empty() && !outcomes.front().accepted);
    assert(foreign.playerMinerals==playerWallet);
    assert(foreign.enemyResearchMinerals()==75);
    injected.kind=RTSCommandKind::TrainCatalog;
    bool rejected=false;
    try{validateRTSCommand(injected);}catch(const std::invalid_argument&){
        rejected=true;
    }
    assert(rejected);

    // A destroyed research structure cancels research and refunds the AI
    // exactly once, without granting owner-wide technology.
    RTSSkirmish ruined;ruined.loadProject(project);
    step(ruined,2);
    assert(ruined.enemyResearchMinerals()==60);
    for(auto& unit:ruined.units)if(unit.id==enemyBase(ruined))
        unit.currentHp=0.f;
    step(ruined,2);
    assert(ruined.enemyResearchMinerals()==75);
    assert(ruined.researchWatch().empty());
    assert(ruined.upgradeLevel(1,"raider_doctrine")==0);
    step(ruined,10);
    assert(ruined.enemyResearchMinerals()==75); // no double refund

    // Default opt-out, AI command ownership and strict author validation.
    RTSProject disabled=project;
    disabled.metadata["enemy_ai_research_enabled"]=picojson::value(false);
    disabled.validate();
    RTSSkirmish noResearch;noResearch.loadProject(disabled);
    step(noResearch,100);
    assert(aiResearchCommands(noResearch.replayTape(),"raider_doctrine")==0);
    assert(noResearch.upgradeLevel(1,"raider_doctrine")==0);
    auto bad=project;
    bad.metadata["enemy_ai_research_interval_ticks"]=picojson::value(1.);
    assert(invalid(bad));
    bad=project;
    bad.metadata["enemy_ai_research_starting_minerals"]=picojson::value(-2.);
    assert(invalid(bad));
    bad=project;
    bad.metadata["enemy_ai_upgrade_priorities"]=picojson::value(
        picojson::array{picojson::value("raider_doctrine"),
                        picojson::value("raider_doctrine")});
    assert(invalid(bad));
    bad=project;
    bad.metadata["enemy_ai_upgrade_priorities"]=picojson::value(
        picojson::array{picojson::value("no_such_upgrade")});
    assert(invalid(bad));

    // F7 controls are atomic, undoable and reject bad schemas.
    EditorSession editor;
    editor.panel=EditorSession::Panel::Data;
    RTSProject authored=project;
    assert(editor.setAIResearchSetting(authored,
        "enemy_ai_research_interval_ticks",picojson::value(45.)));
    assert(rtsAIResearchSettings(authored).intervalTicks==45);
    const auto beforeInvalid=authored.toJson().serialize();
    assert(!editor.setAIResearchSetting(authored,
        "enemy_ai_research_interval_ticks",picojson::value(1.)));
    assert(authored.toJson().serialize()==beforeInvalid);
    assert(!editor.setAIResearchSetting(authored,
        "enemy_ai_upgrade_priorities",picojson::value(
            picojson::array{picojson::value("raider_doctrine"),
                            picojson::value("raider_doctrine")})));
    assert(authored.toJson().serialize()==beforeInvalid);
    assert(editor.undo(authored));
    assert(rtsAIResearchSettings(authored).intervalTicks==30);
    assert(editor.redo(authored));
    assert(rtsAIResearchSettings(authored).intervalTicks==45);

    // Typed catalog rename and removal safety MUST include AI policy
    // references, not just Unit researchable_upgrades and Validator links.
    RTSProject renamed=project;
    assert(renamed.renameCatalogObject("raider_doctrine","raider_drills"));
    renamed.validate();
    const auto policy=rtsAIResearchSettings(renamed);
    assert(policy.upgradePriorities[1]=="raider_drills");
    assert(!renamed.catalogReferences("raider_drills").empty());
    assert(!renamed.removeCatalogObject("raider_drills"));
    // Isolated Upgrade referenced only by AI metadata is still protected.
    RTSProject standalone=RTSProject::defaults();
    CatalogEntry isolated;isolated.id="isolated_doctrine";isolated.type="Upgrade";
    isolated.fields={{"name",picojson::value("Isolated Doctrine")},
                     {"max_level",picojson::value(1.)}};
    assert(standalone.catalog.add(isolated));
    standalone.metadata["enemy_ai_upgrade_priorities"]=picojson::value(
        picojson::array{picojson::value("isolated_doctrine")});
    standalone.validate();
    assert(!standalone.catalogReferences("isolated_doctrine").empty());
    assert(!standalone.removeCatalogObject("isolated_doctrine"));

    assert(project.toJson().serialize()==snapshot);
    std::cout<<"Enemy research planner, budget, two-gate progression, replay, "
                "refund, policy edits and reference integrity passed\n";
}
