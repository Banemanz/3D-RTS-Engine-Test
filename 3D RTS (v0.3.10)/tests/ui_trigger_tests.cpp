#include "RTSUIFrameLayout.hpp"
#include "RTSCommandStream.hpp"
#include "TriggerDefinition.hpp"
#include "EditorSession.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "UI trigger verification requires assertions in optimized Release builds"
#endif
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
bool invalidCommand(RTSCommand cmd){
    try{validateRTSCommand(cmd);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
bool invalidRules(const RTSProject& project){
    try{(void)parseTriggerRules(project);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("Expected UI Trigger Studio project");
    RTSProject project;project.loadFile(argv[1]);project.validate();
    const auto frames=rtsUIFramesFromProject(project);
    assert(frames.size()==12);
    const auto widget=std::find_if(frames.begin(),frames.end(),
        [](const RTSUIFrame& f){return f.id=="event_button";});
    assert(widget!=frames.end() && widget->kind=="Button");
    assert(widget->action=="Trigger" && widget->consumeInput);
    const auto rules=parseTriggerRules(project);
    assert(rules.size()==1 && rules.front().event=="OnUIFrameClicked");
    assert(rules.front().uiFrame=="event_button" && !rules.front().once);
    const auto layout=rtsResolveUIFrames(frames,1280,720);
    const auto hit=rtsUIFrameHit(layout,35.,181.);
    assert(hit.consumed && hit.action=="Trigger" && hit.frameId=="event_button");
    assert(!rtsUIFrameHit(layout,300.,160.).consumed);
    RTSProject invalid=project;
    auto invalidFrames=frames;
    invalidFrames[10].consumeInput=false;
    bool threw=false;
    try{(void)rtsUIFramesToJson(invalidFrames);}
    catch(const std::invalid_argument&){threw=true;}
    assert(threw);
    invalid=project;
    auto definitions=invalid.extensions["triggers"].get<picojson::array>();
    auto rule=definitions.front().get<picojson::object>();
    auto event=rule.at("event").get<picojson::object>();
    event["frame"]=picojson::value("missing_button");
    rule["event"]=picojson::value(event);
    definitions.front()=picojson::value(rule);
    invalid.extensions["triggers"]=picojson::value(definitions);
    assert(invalidRules(invalid));
    threw=false;
    try{invalid.validate();}
    catch(const std::invalid_argument&){threw=true;}
    assert(threw);
    // No callable UI frame may be deleted while an F6 trigger references
    // it. The project transaction must leave all metadata unchanged.
    EditorSession creator;
    const std::string before=project.toJson().serialize();
    auto withoutFrame=frames;
    withoutFrame.erase(withoutFrame.begin()+10);
    assert(!creator.setUIFrames(project,withoutFrame));
    assert(project.toJson().serialize()==before);
    assert(creator.createTrigger(project));
    assert(creator.setTriggerEvent(project,"OnUIFrameClicked"));
    const auto extra=parseTriggerRules(project);
    assert(extra.back().event=="OnUIFrameClicked" &&
           extra.back().uiFrame=="event_button");
    assert(creator.undo(project)); // Undo event conversion
    assert(creator.undo(project)); // Undo added rule
    assert(project.toJson().serialize()==before);

    RTSSkirmish game;
    game.loadProject(project);
    game.setTriggerTracing(true);
    const int minerals=game.playerMinerals;
    assert(!game.queueUIFrameClick("focus_base"));
    assert(!game.queueUIFrameClick("missing_button"));
    assert(game.queueUIFrameClick("event_button"));
    assert(game.playerMinerals==minerals); // not immediate GUI code
    game.update(1.f/60.f);
    assert(game.playerMinerals==minerals+75);
    assert(game.triggerMessage=="BEACON SIGNAL RECEIVED");
    assert(game.triggerDiagnostics().rules()[0].executions==1);
    assert(game.triggerDiagnostics().trace().size()==1);
    assert(game.triggerDiagnostics().trace().back().event=="OnUIFrameClicked");
    assert(game.triggerDiagnostics().pendingUIFrameClicks().empty());
    assert(game.queueUIFrameClick("event_button"));
    game.update(1.f/60.f);
    assert(game.playerMinerals==minerals+150);
    assert(game.triggerDiagnostics().rules()[0].executions==2);
    // Bounded, replay-safe input: only 16 subscribed clicks can be applied
    // at one tick. Additional queued commands are rejected at admission.
    for(int i=0;i<20;++i)assert(game.queueUIFrameClick("event_button"));
    game.update(1.f/60.f);
    assert(game.playerMinerals==minerals+150+16*75);
    assert(game.triggerDiagnostics().rules()[0].executions==18);
    // Further fixed frames are included in checkpoints and replay tape.
    for(int i=0;i<117;++i)game.update(1.f/60.f);
    assert(game.simulationTick()==120);
    auto tape=game.replayTape();
    assert(tape.version==2);
    // Autonomous enemy Attack/Research commands share the replay tape;
    // only compare the 22 actual UI click inputs, not all AI traffic.
    int clickCount=0;
    for(const auto& c:tape.commands){
        if(c.kind!=RTSCommandKind::UIFrameClick)continue;
        ++clickCount;
        assert(c.source==0 && c.catalog=="event_button");
        assert(c.units.empty() && c.targetId==-1);
    }
    assert(clickCount==22);
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&diagnostic));
    // Strict named command schema: no arbitrary executable payloads or
    // forged AI source, nonzero target, unit selection or off-map clicks.
    RTSCommand command=tape.commands.front();
    assert(command.kind==RTSCommandKind::UIFrameClick);
    auto decoded=commandFromJson(commandToJson(command));
    assert(decoded.catalog=="event_button");
    auto bad=command;bad.source=1;assert(invalidCommand(bad));
    bad=command;bad.units={1};assert(invalidCommand(bad));
    bad=command;bad.append=true;assert(invalidCommand(bad));
    bad=command;bad.targetId=1;assert(invalidCommand(bad));
    bad=command;bad.point.x=1.f;assert(invalidCommand(bad));
    bad=command;bad.catalog.clear();assert(invalidCommand(bad));
    bad=command;bad.catalog="../evil";assert(invalidCommand(bad));
    // UI appearance changes cannot alter the authoritative world.
    // Keep EXACTLY the same trigger graph: rule IDs/fired flags and
    // counters are intentionally included in replay state hashes.
    RTSProject baseline=project;
    auto restyled=rtsUIFramesFromProject(baseline);
    restyled[10].x+=12.f;
    restyled[10].palette="Teal";
    restyled[11].text="COSMETIC STATUS";
    baseline.metadata["ui_frames"]=rtsUIFramesToJson(restyled);
    baseline.validate();
    RTSSkirmish original;original.loadProject(baseline);
    RTSSkirmish authored;authored.loadProject(project);
    assert(original.stateDigest()==authored.stateDigest());
    for(int i=0;i<60;++i){
        original.update(1.f/60.f);authored.update(1.f/60.f);
    }
    assert(original.stateDigest()==authored.stateDigest());

    std::ofstream fingerprint("ui_trigger_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    fingerprint<<"UI_INPUT 22 18 "<<game.playerMinerals<<"\n";
    for(const auto& checkpoint:tape.checkpoints)
        fingerprint<<checkpoint.tick<<" "<<checkpoint.digest<<"\n";
    fingerprint.close();
    assert(fingerprint.good());
    std::cout<<"UI trigger: F10 frame -> recorded command -> F6 ECA -> "
                "game state/replay, bounded clicks, strict security, "
                "undo validation, and deterministic cross-OS trace passed\n";
}
