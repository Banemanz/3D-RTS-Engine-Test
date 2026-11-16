#include "RTSUIEditorGeometry.hpp"
#include "EditorSession.hpp"
#include <algorithm>
#include <cassert>
#if defined(NDEBUG)
#error "Responsive UI tests must retain assertions in optimized builds"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
bool close(float a,float b){return std::fabs(a-b)<.001f;}
const RTSUIResolvedFrame& get(const std::vector<RTSUIResolvedFrame>& items,
                             const std::string& id){
    const auto it=std::find_if(items.begin(),items.end(),
        [&](const RTSUIResolvedFrame& f){return f.source.id==id;});
    if(it==items.end())throw std::invalid_argument("Missing resolved UI frame "+id);
    return *it;
}
RTSUIFrame& frame(std::vector<RTSUIFrame>& items,const std::string& id){
    const auto it=std::find_if(items.begin(),items.end(),
        [&](const RTSUIFrame& f){return f.id==id;});
    if(it==items.end())throw std::invalid_argument("Missing author UI frame "+id);
    return *it;
}
bool invalidFrames(const std::vector<RTSUIFrame>& frames){
    try{rtsValidateUIFrames(frames);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
bool invalidProject(const RTSProject& project){
    try{project.validate();}
    catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("Expected responsive F10 showcase path");
    RTSProject project;project.loadFile(argv[1]);project.validate();
    const std::string original=project.toJson().serialize();
    auto frames=rtsUIFramesFromProject(project);
    assert(frames.size()==13);
    assert(frame(frames,"signal_panel").layout=="StackColumn");
    assert(frame(frames,"command_strip").layout=="StackRow");
    assert(frame(frames,"signal_panel").layoutGap==7.f);
    assert(frame(frames,"signal_panel").layoutPadding==12.f);
    assert(frame(frames,"signal_title").stretchCrossAxis);
    assert(frame(frames,"event_button").action=="Trigger");
    assert(rtsUIFramesToJson(frames).serialize()==
           rtsUIFramesToJson(rtsUIFramesFromProject(
               RTSProject::fromJson(project.toJson()))).serialize());

    RTSSkirmish game;game.loadProject(project);
    assert(game.triggerDiagnostics().variables().at("signal_count")==0);
    for(const auto wh:std::vector<std::pair<int,int>>{
            {640,480},{800,600},{1280,720},{1920,1080}}){
        const auto before=rtsResolveUIFrames(frames,wh.first,wh.second,&game);
        const auto& root=get(before,"signal_panel");
        const auto& row=get(before,"command_strip");
        const auto& title=get(before,"signal_title");
        const auto& bank=get(before,"signal_bank");
        const auto& supply=get(before,"signal_supply");
        const auto& event=get(before,"event_button");
        const auto& meter=get(before,"signal_meter");
        const auto& ack=get(before,"signal_ack");
        const auto& counter=get(before,"signal_counter");
        const auto& army=get(before,"army_button");
        const auto& workers=get(before,"worker_button");
        const auto& base=get(before,"focus_base");
        assert(close(root.bounds.x,float(wh.first)-318.f-25.f));
        assert(close(title.bounds.x,root.bounds.x+12.f));
        assert(close(title.bounds.w,root.bounds.w-24.f));
        assert(close(bank.bounds.x,title.bounds.x));
        assert(close(bank.bounds.y,title.bounds.y+title.bounds.h+7.f));
        assert(close(supply.bounds.y,bank.bounds.y+bank.bounds.h+7.f));
        assert(close(event.bounds.y,supply.bounds.y+supply.bounds.h+7.f));
        assert(close(meter.bounds.y,event.bounds.y+event.bounds.h+7.f));
        assert(!ack.visible); // hidden F6 visibility must not consume flow
        assert(close(counter.bounds.y,meter.bounds.y+meter.bounds.h+7.f));
        assert(close(row.bounds.x,25.f));
        assert(close(army.bounds.x,row.bounds.x+10.f));
        assert(close(workers.bounds.x,army.bounds.x+army.bounds.w+9.f));
        assert(close(base.bounds.x,workers.bounds.x+workers.bounds.w+9.f));
        assert(close(workers.bounds.y,army.bounds.y));
        assert(close(base.bounds.y,army.bounds.y));
        // A real mouse click on the already positioned button must target
        // that exact frame, not a fake absolute coordinate from the editor.
        const auto hit=rtsUIFrameHit(before,event.bounds.x+8.,
                                     event.bounds.y+8.);
        assert(hit.consumed && hit.frameId=="event_button" &&
               hit.action=="Trigger");
        if(wh.first>=800){
            const auto armyHit=rtsUIFrameHit(before,army.bounds.x+4.,
                                             army.bounds.y+4.);
            assert(armyHit.consumed && armyHit.action=="SelectArmy");
        }
    }

    // Once the authored F6 event fires, one previously hidden Label becomes
    // visible and following siblings shift by its height + configured gap.
    const auto previous=rtsResolveUIFrames(frames,1280,720,&game);
    const float oldCounter=get(previous,"signal_counter").bounds.y;
    assert(game.queueUIFrameClick("event_button"));
    game.update(1.f/60.f);
    assert(game.triggerDiagnostics().variables().at("signal_count")==1);
    assert(game.triggerMessage=="BEACON SIGNAL RECEIVED");
    assert(rtsUIFrameText(frame(frames,"signal_status"),game)==
           game.triggerMessage);
    const auto after=rtsResolveUIFrames(frames,1280,720,&game);
    const auto& ack=get(after,"signal_ack");
    const auto& counter=get(after,"signal_counter");
    assert(ack.visible);
    assert(close(ack.bounds.y,
                 get(after,"signal_meter").bounds.y+
                 get(after,"signal_meter").bounds.h+7.f));
    assert(close(counter.bounds.y,oldCounter+ack.bounds.h+7.f));
    assert(close(counter.bounds.y,ack.bounds.y+ack.bounds.h+7.f));
    assert(close(get(previous,"event_button").bounds.y,
                 get(after,"event_button").bounds.y));
    assert(rtsUIFrameBarRatio(frame(frames,"signal_meter"),game)>0.f);

    // Extra invisible preceding children must NOT reserve a phantom gap,
    // and a child outside its own parent must not draw or consume input.
    auto trial=frames;
    auto& clipped=frame(trial,"command_strip");
    clipped.width=90.f; // small bounded parent clips later row buttons
    const auto overflow=rtsResolveUIFrames(trial,1280,720,&game);
    const auto& worker=get(overflow,"worker_button");
    assert(worker.clipped.w<worker.bounds.w);
    assert(rtsUIFrameHit(overflow,worker.bounds.x+5,worker.bounds.y+5)
                        .frameId!="worker_button");
    const auto& toolbar=get(overflow,"command_strip");
    assert(close(toolbar.bounds.w,90.f));

    // Strict declarative schema: no scripts, impossible gap/padding,
    // non-Panel layout, illegal stretch or invalid serialized types.
    trial=frames;frame(trial,"signal_panel").layout="Something";assert(invalidFrames(trial));
    trial=frames;frame(trial,"signal_title").layout="StackRow";assert(invalidFrames(trial));
    trial=frames;frame(trial,"signal_panel").layoutGap=65.f;assert(invalidFrames(trial));
    trial=frames;frame(trial,"signal_panel").layoutPadding=-1.f;assert(invalidFrames(trial));
    trial=frames;frame(trial,"signal_panel").layout="Free";assert(invalidFrames(trial));
    trial=frames;frame(trial,"signal_title").parent.clear();assert(invalidFrames(trial));
    trial=frames;frame(trial,"signal_title").stretchCrossAxis=false;
    frame(trial,"signal_title").parent.clear();rtsValidateUIFrames(trial);
    trial=frames;frame(trial,"signal_panel").layoutPadding=std::nanf("");
    assert(invalidFrames(trial));
    auto bad=project;
    auto metadata=bad.metadata["ui_frames"].get<picojson::array>();
    metadata[0].get<picojson::object>()["layout_gap"]=picojson::value("huge");
    bad.metadata["ui_frames"]=picojson::value(metadata);
    assert(invalidProject(bad));
    bad=project;metadata=bad.metadata["ui_frames"].get<picojson::array>();
    metadata[1].get<picojson::object>()["stretch_cross_axis"]=picojson::value(3.);
    bad.metadata["ui_frames"]=picojson::value(metadata);
    assert(invalidProject(bad));

    // Reflow is a presentation-only choice. The exact trigger graph and
    // authored buttons remain untouched, so the authoritative game state
    // and replay digests must be identical for free vs stacked panels.
    RTSProject freeform=project;
    auto freeFrames=rtsUIFramesFromProject(freeform);
    frame(freeFrames,"signal_panel").layout="Free";
    frame(freeFrames,"command_strip").layout="Free";
    for(auto& f:freeFrames)f.stretchCrossAxis=false;
    freeform.metadata["ui_frames"]=rtsUIFramesToJson(freeFrames);
    freeform.validate();
    RTSSkirmish withFlow,withoutFlow;
    withFlow.loadProject(project);withoutFlow.loadProject(freeform);
    assert(withFlow.stateDigest()==withoutFlow.stateDigest());
    for(int i=0;i<120;++i){
        withFlow.update(1.f/60.f);
        withoutFlow.update(1.f/60.f);
        if((i+1)%30==0)assert(withFlow.stateDigest()==withoutFlow.stateDigest());
    }
    const auto tape=withFlow.replayTape();
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&diagnostic));
    std::ofstream fingerprint("ui_responsive_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    for(const auto& c:tape.checkpoints)
        fingerprint<<c.tick<<" "<<c.digest<<"\n";
    fingerprint.flush();assert(fingerprint.good());

    // F10 operations are validated, atomic and undoable.
    EditorSession editor;editor.panel=EditorSession::Panel::Interface;
    RTSProject authored=project;
    auto changed=frames;
    frame(changed,"signal_panel").layoutGap=12.f;
    frame(changed,"command_strip").layoutPadding=20.f;
    assert(editor.setUIFrames(authored,changed));
    assert(rtsUIFramesFromProject(authored)[0].layoutGap==12.f);
    const auto valid=authored.toJson().serialize();
    assert(!editor.setUIFrames(authored,trial)); // invalid NaN
    assert(authored.toJson().serialize()==valid);
    assert(editor.undo(authored));
    assert(authored.toJson().serialize()==original);
    assert(editor.redo(authored));
    assert(rtsUIFramesFromProject(authored)[0].layoutGap==12.f);
    std::cout<<"Responsive F10 UI row/column layout: stable anchoring, "
                "hidden-sibling reflow, stretch and clip, real click geometry, "
                "strict schema, undo, state digest and replay passed\n";
}
