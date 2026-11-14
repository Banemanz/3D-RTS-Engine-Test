#include "RTSAIPlanner.hpp"
#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdexcept>

int main(int argc,char** argv){
    RTSSkirmish game;
    const int homeId=game.countUnitsForTrigger(1,"rift_bastion");
    assert(homeId==1);
    int fortress=-1,objective=game.playerBaseId();
    std::vector<int> attackers;
    for(auto& unit:game.units){
        if(unit.ownerId==1 && unit.isStructure)fortress=unit.id;
        else if(unit.ownerId==1 && !unit.isStructure){
            unit.state=UnitState::IDLE;attackers.push_back(unit.id);
        }
    }
    assert(fortress>0 && objective>0 && attackers.size()>=2);
    const RTSAICombatPolicy strategic{true,0.f};
    auto plan=planRTSEnemyAttackOrders(game.units,1,fortress,objective,25,strategic);
    assert(plan.size()==attackers.size());
    for(std::size_t i=0;i<plan.size();++i){
        const auto& action=plan[i];
        assert(action.source==1 && action.kind==RTSCommandKind::Attack);
        assert(action.tick==25 && action.targetId==objective &&
               action.units==std::vector<int>{attackers[i]});
        validateRTSCommand(action);
    }
    auto reversed=game.units;
    std::reverse(reversed.begin(),reversed.end());
    assert(planRTSEnemyAttackOrders(reversed,1,fortress,objective,25,strategic)
               .size()==plan.size());
    const auto reversedPlan=planRTSEnemyAttackOrders(reversed,1,fortress,objective,25,strategic);
    for(std::size_t i=0;i<plan.size();++i)
        assert(plan[i].units==reversedPlan[i].units &&
               plan[i].targetId==reversedPlan[i].targetId);

    auto home=std::find_if(game.units.begin(),game.units.end(),
        [&](const Unit& unit){return unit.id==fortress;});
    assert(home!=game.units.end());
    int forwardTarget=-1,backTarget=-1;
    for(auto& unit:game.units)if(unit.ownerId==0 && !unit.isStructure){
        if(forwardTarget<0){
            forwardTarget=unit.id;
            unit.position={home->position.x+3.f,0,home->position.z};
        }else if(backTarget<0){
            backTarget=unit.id;
            unit.position={home->position.x-5.f,0,home->position.z};
        }
    }
    assert(forwardTarget>0 && backTarget>0);
    const auto defensive=planRTSEnemyAttackOrders(
        game.units,1,fortress,objective,25,{true,12.f});
    assert(defensive.size()==plan.size());
    for(const auto& command:defensive)
        assert(command.targetId==forwardTarget || command.targetId==backTarget);
    assert(planRTSEnemyAttackOrders(game.units,1,fortress,objective,25,{false,12.f}).empty());
    assert(planRTSEnemyAttackOrders(game.units,1,fortress,objective,25,{true,0.f})[0]
           .targetId==objective);
    assert(planRTSEnemyAttackOrders(game.units,1,fortress,objective,0,{true,12.f}).empty());
    assert(planRTSEnemyAttackOrders(game.units,0,fortress,objective,25,{true,12.f}).empty());
    // No edits to the passed Unit collection: the planner merely proposes.
    for(int id:attackers){
        const auto it=std::find_if(game.units.begin(),game.units.end(),
            [&](const Unit& u){return u.id==id;});
        assert(it!=game.units.end() && it->state==UnitState::IDLE);
    }
    RTSProject disabled=RTSProject::defaults();
    disabled.metadata["enemy_ai_enabled"]=picojson::value(false);
    disabled.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    RTSSkirmish disabledGame;
    disabledGame.loadProject(disabled);
    for(int i=0;i<120;++i)disabledGame.update(1.f/60.f);
    assert(disabledGame.replayTape().commands.empty());
    assert(verifyRTSReplay(disabledGame.replayTape()));
    if(argc>1){
        RTSProject scenario;
        scenario.loadFile(argv[1]);
        RTSSkirmish match;
        match.loadProject(scenario);
        int scout=-1,decoy=-1;
        for(const auto& unit:match.units){
            if(unit.instanceName=="Frontier Scout")scout=unit.id;
            if(unit.instanceName=="Frontier Decoy")decoy=unit.id;
        }
        assert(scout>0 && decoy>0);
        match.update(1.f/60.f);
        match.update(1.f/60.f);
        const auto startup=match.replayTape();
        bool choseDefense=false;
        for(const auto& command:startup.commands)
            if(command.source==1 && (command.targetId==scout ||
                                      command.targetId==decoy))
                choseDefense=true;
        assert(choseDefense);
        for(int tick=2;tick<180;++tick)match.update(1.f/60.f);
        const auto recorded=match.replayTape();
        assert(recorded.version==2 && recorded.totalTicks==180);
        assert(verifyRTSReplay(recorded));
        std::ofstream fingerprints("ai_defense_fingerprint.txt",
                                   std::ios::binary|std::ios::trunc);
        assert(fingerprints.is_open());
        for(const auto& point:recorded.checkpoints)
            fingerprints<<point.tick<<" "<<point.digest<<"\n";
        fingerprints.flush();
        assert(fingerprints.good());
    }
    std::cout<<"AI policy: deterministic unit IDs, local defense, opt-out and replay passed\n";
}
