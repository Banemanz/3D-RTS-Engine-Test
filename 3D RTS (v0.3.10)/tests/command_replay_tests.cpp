#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <stdexcept>

int main(){
    RTSProject project=RTSProject::defaults();
    project.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    RTSSkirmish game;
    game.loadProject(project);
    const auto starting=game.stateDigest();
    assert(starting.size()==16);
    const auto select=[&](const std::string& catalog){
        for(auto& u:game.units)u.isSelected=(u.ownerId==0 && u.catalogId==catalog &&
                                            u.isAlive());
    };
    select("ranger");
    assert(game.queuePlayerCommand(RTSCommandKind::Move,{-14.f,0.f,20.f}));
    // Alter visual selection after command submission. Dispatch must use
    // the original captured unit identities, not the new UI state.
    select("worker");
    for(int i=0;i<35;++i)game.update(1.f/60.f);
    assert(game.simulationTick()==35);
    select("command_center");
    assert(game.queuePlayerCommand(RTSCommandKind::TrainCatalog,{},-1,false,"worker"));
    for(int i=0;i<35;++i)game.update(1.f/60.f);
    select("ranger");
    assert(game.queuePlayerCommand(RTSCommandKind::AttackMove,{0.f,0.f,17.f}));
    for(int i=0;i<70;++i)game.update(1.f/60.f);
    assert(game.queuePlayerCommand(RTSCommandKind::Stop));
    for(int i=0;i<65;++i)game.update(1.f/60.f);
    assert(game.simulationTick()==205);
    const auto tape=game.replayTape();
    assert(tape.totalTicks==205);
    assert(tape.commands.size()==4);
    assert(tape.commands[0].tick==1);
    assert(tape.commands[1].tick==36);
    assert(tape.commands[2].tick==71);
    assert(tape.commands[3].tick==141);
    assert(tape.checkpoints.front().tick==0);
    assert(tape.checkpoints.back().tick==205);
    assert(tape.checkpoints.front().digest==starting);
    assert(tape.checkpoints.back().digest==game.stateDigest());
    // Cross-compiler parity artifact: compare *every* state checkpoint from
    // the same scripted inputs in the Windows and Linux CI jobs.
    {
        std::ofstream fingerprint("command_replay_fingerprint.txt",
                                  std::ios::binary|std::ios::trunc);
        assert(fingerprint.is_open());
        for(const auto& check:tape.checkpoints)
            fingerprint<<check.tick<<" "<<check.digest<<"\n";
        fingerprint.flush();
        assert(fingerprint.good());
    }
    std::string reason;
    assert(verifyRTSReplay(tape,&reason));
    assert(reason.find("verified")!=std::string::npos);

    // Direct mutation API is still available to integrations, but cannot
    // masquerade as a complete replay recording.
    RTSSkirmish untracked;
    untracked.loadProject(project);
    for(auto& u:untracked.units)
        u.isSelected=u.ownerId==0 && u.catalogId=="ranger";
    assert(untracked.orderMove({-14.f,0.f,20.f},false));
    assert(!untracked.replayRecordingValid());
    bool invalidExport=false;
    try{(void)untracked.replayTape();}catch(const std::runtime_error&){invalidExport=true;}
    assert(invalidExport);
    // Failed actions are still ordered simulation inputs, but their result
    // can be shown to the creator without putting UI events in game state.
    RTSSkirmish denied;
    denied.loadProject(project);
    for(auto& u:denied.units)
        u.isSelected=u.ownerId==0 && u.catalogId=="command_center";
    assert(denied.queuePlayerCommand(RTSCommandKind::TrainCatalog,{},-1,false,
                                     "nonexistent_unit"));
    denied.update(1.f/60.f);
    const auto outcome=denied.takeCommandOutcomes();
    assert(outcome.size()==1 && !outcome[0].accepted &&
           outcome[0].kind==RTSCommandKind::TrainCatalog &&
           outcome[0].tick==1);
    assert(denied.takeCommandOutcomes().empty());
    assert(verifyRTSReplay(denied.replayTape()));

    const RTSReplayTape encoded=RTSReplayTape::fromJson(tape.toJson());
    assert(verifyRTSReplay(encoded));
    const std::string path="command_replay_regression.rtsreplay.json";
    tape.saveFile(path);
    const auto fromDisk=RTSReplayTape::loadFile(path);
    // Keep a tiny test fixture in the build directory for the separate
    // RTSProjectTool --verify-replay CTest invocation.
    assert(verifyRTSReplay(fromDisk));
    assert(fromDisk.commands.size()==4);

    auto changed=fromDisk;
    changed.commands[0].point.z=-35.f;
    assert(!verifyRTSReplay(changed,&reason));
    assert(reason.find("diverged")!=std::string::npos);
    changed=fromDisk;
    changed.checkpoints.back().digest="0000000000000000";
    assert(!verifyRTSReplay(changed,&reason));

    RTSCommandStream stream;
    RTSCommand action=fromDisk.commands[0];
    action.sequence=11;action.tick=1;
    assert(stream.enqueue(action,0));
    assert(!stream.enqueue(action,0)); // no duplicate player/sequence
    action.sequence=10;
    assert(stream.enqueue(action,0));
    const auto ordered=stream.take(1);
    assert(ordered.size()==2 && ordered[0].sequence==10 &&
           ordered[1].sequence==11 && stream.pending()==0);
    action.tick=1;
    assert(!stream.enqueue(action,1)); // no late commands
    action.tick=1+RTSCommandStream::MaxAhead+1;
    assert(!stream.enqueue(action,0)); // no unbounded future queue
    stream.clear();
    action.tick=RTSReplayTape::MaxTicks+1;
    action.units={2};
    assert(stream.enqueue(action,RTSReplayTape::MaxTicks)); // game stays playable
    assert(stream.take(RTSReplayTape::MaxTicks+1).size()==1);
    action.tick=2;action.units={9,9};
    bool rejected=false;
    try{(void)stream.enqueue(action,0);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected); // duplicate unit references rejected, never silently normalized
    rejected=false;
    try{
        auto invalid=tape.toJson();
        auto& values=invalid.get<picojson::object>().at("commands").get<picojson::array>();
        values[1].get<picojson::object>()["tick"]=picojson::value(0.0);
        (void)RTSReplayTape::fromJson(invalid);
    }catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::cout<<"command replay: fixed-tick dispatch, selection capture, JSON roundtrip, tampering and queue bounds passed\n";
}
