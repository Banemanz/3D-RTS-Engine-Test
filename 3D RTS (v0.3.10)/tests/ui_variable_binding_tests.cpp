#include "RTSUIFrameLayout.hpp"
#include "TriggerDefinition.hpp"
#include "EditorSession.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "UI variable tests must retain assertions in Release"
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
const RTSUIFrame& get(const std::vector<RTSUIFrame>& frames,
                      const std::string& id){
    const auto it=std::find_if(frames.begin(),frames.end(),
        [&](const RTSUIFrame& f){return f.id==id;});
    if(it==frames.end())throw std::invalid_argument("missing frame "+id);
    return *it;
}
RTSUIFrame& get(std::vector<RTSUIFrame>& frames,const std::string& id){
    const auto it=std::find_if(frames.begin(),frames.end(),
        [&](const RTSUIFrame& f){return f.id==id;});
    if(it==frames.end())throw std::invalid_argument("missing frame "+id);
    return *it;
}
const RTSUIResolvedFrame& find(const std::vector<RTSUIResolvedFrame>& frames,
                               const std::string& id){
    const auto it=std::find_if(frames.begin(),frames.end(),
        [&](const RTSUIResolvedFrame& f){return f.source.id==id;});
    if(it==frames.end())throw std::invalid_argument("missing resolved "+id);
    return *it;
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
bool invalidFrames(const std::vector<RTSUIFrame>& frames){
    try{rtsValidateUIFrames(frames);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("Expected UI variable dashboard sample path");
    RTSProject project;project.loadFile(argv[1]);project.validate();
    const auto source=project.toJson().serialize();
    const auto frames=rtsUIFramesFromProject(project);
    assert(frames.size()==15);
    assert(get(frames,"signal_meter").binding=="Variable");
    assert(get(frames,"signal_meter").variable=="signal_count");
    assert(get(frames,"signal_meter").barMin==0);
    assert(get(frames,"signal_meter").barMax==3);
    assert(get(frames,"signal_ack").visibleVariable=="signal_count");
    assert(get(frames,"signal_ack").visibleAtLeast==1);
    assert(parseTriggerVariables(project).at("signal_count")==0);
    assert(RTSProject::fromJson(project.toJson()).toJson().serialize()==source);
    assert(rtsUIFramesToJson(frames).serialize()==
           rtsUIFramesToJson(rtsUIFramesFromProject(
               RTSProject::fromJson(project.toJson()))).serialize());

    RTSSkirmish game;game.loadProject(project);
    assert(game.triggerDiagnostics().variables().at("signal_count")==0);
    assert(rtsUIFrameBarRatio(get(frames,"signal_meter"),game)==0.f);
    assert(rtsUIFrameText(get(frames,"signal_counter"),game)=="BEACONS 0");
    const auto before=rtsResolveUIFrames(frames,1280,720,&game);
    const auto& ackHidden=find(before,"signal_ack");
    assert(!ackHidden.visible);
    assert(rtsUIFrameAt(before,ackHidden.bounds.x+8,ackHidden.bounds.y+8)<0);
    assert(!rtsUIFrameHit(before,ackHidden.bounds.x+8,ackHidden.bounds.y+8).consumed);
    assert(game.queueUIFrameClick("event_button"));
    const int minerals=game.playerMinerals;
    game.update(1.f/60.f);
    assert(game.playerMinerals==minerals+75);
    assert(game.triggerDiagnostics().variables().at("signal_count")==1);
    assert(std::fabs(rtsUIFrameBarRatio(get(frames,"signal_meter"),game)-1.f/3.f)<.0001f);
    assert(rtsUIFrameText(get(frames,"signal_counter"),game)=="BEACONS 1");
    auto after=rtsResolveUIFrames(frames,1280,720,&game);
    assert(find(after,"signal_ack").visible);
    assert(rtsUIFrameAt(after,find(after,"signal_ack").bounds.x+4,
                              find(after,"signal_ack").bounds.y+4)>=0);
    // Repeated real click events advance the same deterministic variable
    // and saturate only the presentation progress bar (never the F6 variable).
    for(int i=0;i<4;++i){
        assert(game.queueUIFrameClick("event_button"));
        game.update(1.f/60.f);
    }
    assert(game.triggerDiagnostics().variables().at("signal_count")==5);
    assert(rtsUIFrameBarRatio(get(frames,"signal_meter"),game)==1.f);
    assert(rtsUIFrameText(get(frames,"signal_counter"),game)=="BEACONS 5");

    // Visibility is inherited by descendants. If the parent is hidden,
    // no child can draw or receive clicks even though its own threshold
    // might pass. F10 hierarchy retains the authored frame definition.
    auto hidden=frames;
    get(hidden,"mission_console").visibleVariable="signal_count";
    get(hidden,"mission_console").visibleAtLeast=6;
    const auto tree=rtsResolveUIFrames(hidden,1280,720,&game);
    assert(!find(tree,"mission_console").visible);
    assert(!find(tree,"army_button").visible);
    const auto& army=find(tree,"army_button");
    assert(!rtsUIFrameHit(tree,army.bounds.x+4,army.bounds.y+4).consumed);
    for(const auto size:std::vector<std::pair<int,int>>{
            {640,480},{800,600},{1280,720},{1920,1080}}){
        const auto lay=rtsResolveUIFrames(frames,size.first,size.second,&game);
        assert(find(lay,"signal_ack").visible);
        assert(find(lay,"signal_meter").clipped.w>0.f);
        assert(find(lay,"signal_ack").clipped.x>=0.f);
    }

    // Authoring strictness: bound/visible names and integer ranges cannot
    // become stale, NaN, arbitrary expressions, or non-F6 data references.
    RTSProject broken=project;
    auto edit=rtsUIFramesFromProject(broken);
    get(edit,"signal_meter").variable="does_not_exist";
    broken.metadata["ui_frames"]=rtsUIFramesToJson(edit);
    assert(invalid(broken));
    edit=frames;get(edit,"signal_ack").visibleVariable="does_not_exist";
    broken=project;broken.metadata["ui_frames"]=rtsUIFramesToJson(edit);
    assert(invalid(broken));
    edit=frames;get(edit,"signal_meter").barMax=0;
    assert(invalidFrames(edit));
    edit=frames;get(edit,"signal_meter").barMin=100000001;
    assert(invalidFrames(edit));
    edit=frames;get(edit,"signal_meter").variable="";
    assert(invalidFrames(edit));
    edit=frames;get(edit,"signal_counter").binding="Minerals";
    assert(invalidFrames(edit)); // stale Variable field
    broken=project;
    auto raw=broken.metadata["ui_frames"].get<picojson::array>();
    auto item=raw.back().get<picojson::object>();
    item["visible_at_least"]=picojson::value(0.5);
    raw.back()=picojson::value(item);
    broken.metadata["ui_frames"]=picojson::value(raw);
    assert(invalid(broken));
    broken=project;
    broken.extensions["trigger_variables"]=picojson::value(picojson::object{});
    assert(invalid(broken));

    // F6 variable rename migrates BOTH its typed ECA operands and the F10
    // binding + visibility references, preserving free-form user text.
    EditorSession editor;editor.panel=EditorSession::Panel::Triggers;
    const auto original=project.toJson().serialize();
    assert(!editor.removeTriggerVariable(project,"signal_count"));
    assert(project.toJson().serialize()==original);
    assert(editor.renameTriggerVariable(project,"signal_count","uplink_level"));
    auto renamed=rtsUIFramesFromProject(project);
    assert(get(renamed,"signal_meter").variable=="uplink_level");
    assert(get(renamed,"signal_counter").variable=="uplink_level");
    assert(get(renamed,"signal_ack").visibleVariable=="uplink_level");
    assert(get(renamed,"signal_counter").text=="BEACONS");
    assert(parseTriggerVariables(project).count("signal_count")==0);
    assert(parseTriggerVariables(project).count("uplink_level")==1);
    const auto rules=parseTriggerRules(project);
    assert(rules.size()==1 && rules.front().event=="OnUIFrameClicked");
    assert(editor.undo(project));
    assert(project.toJson().serialize()==original);
    assert(editor.redo(project));
    assert(get(rtsUIFramesFromProject(project),"signal_meter").variable=="uplink_level");

    // UI variable visibility and binding are read-only. Restyling their
    // geometry, labels, thresholds/range does not mutate authoritative
    // state or break deterministic replay.
    RTSSkirmish compared;
    RTSProject appearance=project;
    auto restyled=rtsUIFramesFromProject(appearance);
    get(restyled,"signal_meter").width+=17.f;
    get(restyled,"signal_ack").visibleAtLeast=3;
    appearance.metadata["ui_frames"]=rtsUIFramesToJson(restyled);
    appearance.validate();
    compared.loadProject(appearance);
    RTSSkirmish originalGame;originalGame.loadProject(project);
    assert(compared.stateDigest()==originalGame.stateDigest());
    for(int i=0;i<75;++i){
        compared.update(1.f/60.f);
        originalGame.update(1.f/60.f);
        if(i%15==14)assert(compared.stateDigest()==originalGame.stateDigest());
    }
    for(int i=5;i<120;++i)game.update(1.f/60.f);
    assert(game.simulationTick()==120);
    auto tape=game.replayTape();
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&diagnostic));
    std::ofstream out("ui_variable_fingerprint.txt",std::ios::binary|std::ios::trunc);
    assert(out.is_open());
    out<<"VARIABLE 5 "<<game.triggerDiagnostics().variables().at("signal_count")<<"\n";
    for(const auto& checkpoint:tape.checkpoints)
        out<<checkpoint.tick<<" "<<checkpoint.digest<<"\n";
    out.flush();
    assert(out.good());
    std::cout<<"F6->F10 variable-bound HUD, conditional UI visibility, "
                "parent clipping, typed rename/deletion integrity, "
                "replay and owner-independent state verified\n";
}
