#include "RTSUIFrameLayout.hpp"
#include "EditorSession.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "UI frame tests require active assertions even in Release"
#endif
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
bool invalid(const std::vector<RTSUIFrame>& frames){
    try{rtsValidateUIFrames(frames);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
bool invalidProject(const RTSProject& p){
    try{p.validate();}
    catch(const std::invalid_argument&){return true;}
    return false;
}
const RTSUIResolvedFrame& byId(const std::vector<RTSUIResolvedFrame>& frames,
                              const std::string& id){
    const auto found=std::find_if(frames.begin(),frames.end(),
        [&](const RTSUIResolvedFrame& f){return f.source.id==id;});
    if(found==frames.end())throw std::invalid_argument("Missing frame: "+id);
    return *found;
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("Expected original UI frame showcase map");
    RTSProject legacy=RTSProject::defaults();
    assert(rtsUIFramesFromProject(legacy).empty());
    assert(rtsUIFrameHit({},500.,300.).consumed==false);
    const auto oldHUD=RTSHUDLayout(1280,720);
    RTSProject project;project.loadFile(argv[1]);project.validate();
    const auto frames=rtsUIFramesFromProject(project);
    assert(frames.size()==10);
    assert(frames[0].id=="mission_console" && frames[0].consumeInput);
    assert(frames[1].parent=="mission_console");
    assert(frames[8].parent=="status_footer");
    assert(frames[9].kind=="Button" && frames[9].action=="FocusBase");
    assert(project.toJson().serialize()==
           RTSProject::fromJson(project.toJson()).toJson().serialize());
    const auto round=rtsUIFramesFromProject(RTSProject::fromJson(project.toJson()));
    assert(rtsUIFramesToJson(frames).serialize()==rtsUIFramesToJson(round).serialize());
    RTSSkirmish game;game.loadProject(project);
    assert(rtsUIFrameText(frames[2],game).find(std::to_string(game.playerMinerals))
           !=std::string::npos);
    assert(rtsUIFrameText(frames[3],game).find("/")!=std::string::npos);
    assert(rtsUIFrameBarRatio(frames[4],game)==0.f);
    for(auto& u:game.units)if(u.ownerId==0 && u.isAlive() &&
                              !u.isStructure)u.isSelected=true;
    assert(rtsUIFrameBarRatio(frames[4],game)>0.99f);
    assert(rtsUIFrameText(frames[8],game).find("DAY TICK")!=std::string::npos);
    for(const auto size:std::vector<std::pair<int,int>>{
            {640,480},{800,600},{1280,720},{1920,1080}}){
        const auto resolved=rtsResolveUIFrames(frames,size.first,size.second);
        assert(resolved.size()==frames.size());
        const auto& mission=byId(resolved,"mission_console");
        assert(std::fabs(mission.bounds.x-(size.first-304.f-25.f))<.001f);
        assert(mission.bounds.y==110.f);
        assert(mission.depth==0);
        const auto& header=byId(resolved,"header");
        assert(header.depth==1 && header.bounds.x==mission.bounds.x+13.f);
        const auto& footer=byId(resolved,"status_footer");
        assert(footer.depth==1 && footer.bounds.y>mission.bounds.y+185.f);
        const auto& status=byId(resolved,"status_label");
        assert(status.depth==2 && status.clipped.h>0.f);
        const auto& army=byId(resolved,"army_button");
        const auto click=rtsUIFrameHit(resolved,army.bounds.x+5.,army.bounds.y+5.);
        assert(click.consumed && click.action=="SelectArmy");
        assert(click.frameId=="army_button");
        const auto& workers=byId(resolved,"worker_button");
        assert(rtsUIFrameHit(resolved,workers.bounds.x+5.,workers.bounds.y+5.)
                   .action=="SelectWorkers");
        const auto& base=byId(resolved,"focus_base");
        assert(rtsUIFrameHit(resolved,base.bounds.x+4.,base.bounds.y+4.)
                   .action=="FocusBase");
        const auto background=rtsUIFrameHit(resolved,mission.bounds.x+5,
                                            mission.bounds.y+5);
        assert(background.consumed && background.action=="None");
        assert(!rtsUIFrameHit(resolved,size.first*.45,size.second*.50).consumed);
        assert(rtsUIFrameAt(resolved,header.bounds.x+7,header.bounds.y+7)>=0);
        assert(oldHUD.commandArea.w==330.f); // old built-in HUD is untouched
    }
    // Ancestor clipping protects both drawn bounds and input. A child may
    // extend beyond its parent, but the spill can never receive a click.
    auto overflow=frames;
    overflow[5].x=260.f; // button extends 126px outside its 304px parent
    const auto clipped=rtsResolveUIFrames(overflow,1280,720);
    const auto& parent=byId(clipped,"mission_console");
    const auto& child=byId(clipped,"army_button");
    assert(child.clipped.w<child.bounds.w);
    assert(child.clipped.x+child.clipped.w<=parent.bounds.x+parent.bounds.w);
    assert(rtsUIFrameHit(clipped,child.bounds.x+child.bounds.w-3,
                         child.bounds.y+5).frameId!="army_button");

    // Strict bounded schema, parent ordering, actions and text safety.
    auto bad=frames;bad[1].id=frames[0].id;assert(invalid(bad));
    bad=frames;bad[1].parent="future";assert(invalid(bad));
    bad=frames;bad[0].parent=bad[0].id;assert(invalid(bad));
    bad=frames;bad[1].action="SelectArmy";assert(invalid(bad));
    bad=frames;bad[4].binding="Minerals";assert(invalid(bad));
    bad=frames;bad[5].action="RunScript";assert(invalid(bad));
    bad=frames;bad[5].width=std::nanf("");assert(invalid(bad));
    bad=frames;bad[2].text=std::string(129,'A');assert(invalid(bad));
    bad=frames;bad[2].text="bad\nlabel";assert(invalid(bad));
    bad=frames;bad[3].palette="Unknown";assert(invalid(bad));
    bad=frames;bad[5].anchor="Freeform";assert(invalid(bad));
    bad=frames;while(bad.size()<33){auto f=frames[1];f.id="ui_"+std::to_string(bad.size());bad.push_back(f);}
    assert(invalid(bad));
    auto invalidJSON=project;
    invalidJSON.metadata["ui_frames"]=picojson::value("code");
    assert(invalidProject(invalidJSON));
    invalidJSON=project;
    picojson::array malformed;
    malformed.emplace_back(picojson::value(picojson::object{
        {"id",picojson::value("x")},{"kind",picojson::value("Button")},
        {"action",picojson::value("RunScript")} }));
    invalidJSON.metadata["ui_frames"]=picojson::value(malformed);
    assert(invalidProject(invalidJSON));

    // Editor operations must be one atomic transaction, with complete
    // project rollback on rejected values and correct undo/redo.
    EditorSession ed;
    ed.panel=EditorSession::Panel::Interface;
    const std::string serialized=project.toJson().serialize();
    auto modified=frames;
    modified[5].x=61.f;
    assert(ed.setUIFrames(project,modified));
    assert(ed.modified);
    assert(rtsUIFramesFromProject(project)[5].x==61.f);
    assert(!ed.setUIFrames(project,modified));
    const std::string after=project.toJson().serialize();
    assert(!ed.setUIFrames(project,bad));
    assert(project.toJson().serialize()==after);
    assert(ed.undo(project));
    assert(project.toJson().serialize()==serialized);
    assert(ed.redo(project));
    assert(rtsUIFramesFromProject(project)[5].x==61.f);
    assert(ed.setUIFrames(project,{}));
    assert(project.metadata.find("ui_frames")==project.metadata.end());
    assert(ed.undo(project));
    assert(rtsUIFramesFromProject(project).size()==10);
    std::cout<<"UI frame hierarchy: rendering/input geometry, nested clipping, "
                "live bindings, legal buttons, strict metadata, undo/redo "
                "and classic HUD compatibility passed\n";
}
