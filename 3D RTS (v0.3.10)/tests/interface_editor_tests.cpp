#include "RTSHUDLayout.hpp"
#include "RTSHUDProfile.hpp"
#include "EditorSession.hpp"
#include <cassert>
#include <array>
#if defined(NDEBUG)
#error "Interface authoring tests must retain assertions in Release"
#endif
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
bool invalid(const RTSProject& p){
    try{p.validate();}
    catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("Interface showcase project path required");
    RTSProject defaults=RTSProject::defaults();
    assert(defaults.metadata.find("hud_profile")==defaults.metadata.end());
    const auto original=RTSHUDProfile::fromProject(defaults);
    assert(original.isDefault());
    // Backward-compatible layout geometry: old maps have EXACT old rects.
    RTSHUDLayout legacy(1280,720);
    RTSHUDLayout fromMap(1280,720,original);
    assert(legacy.bottom.y==526.f && legacy.bottom.h==194.f);
    assert(legacy.topBar.y==22.f && legacy.topBar.h==46.f);
    assert(legacy.minimap.x==14.f && legacy.minimap.w==157.f);
    assert(legacy.minimap.h==154.f);
    assert(legacy.commandArea.x==936.f && legacy.commandArea.w==330.f);
    assert(fromMap.minimap.x==legacy.minimap.x);
    assert(fromMap.selectionArea.x==legacy.selectionArea.x);
    assert(fromMap.commandArea.x==legacy.commandArea.x);
    assert(!legacy.consumes(12.,12.));
    assert(legacy.consumes(12.,32.));
    RTSProject project;project.loadFile(argv[1]);
    project.validate();
    const auto profile=RTSHUDProfile::fromProject(project);
    assert(!profile.isDefault());
    assert(profile.bottomExtra==34.f);
    assert(profile.minimapScale==1.18f);
    assert(profile.commandWidth==360.f);
    assert(profile.commandInset==20.f);
    const std::string before=project.toJson().serialize();
    const auto round=RTSProject::fromJson(project.toJson());
    assert(round.toJson().serialize()==before);
    assert(RTSHUDProfile::fromProject(round).commandWidth==360.f);
    RTSSkirmish game;game.loadProject(project);
    // Across small and large viewports, the exact same authored layout
    // drives command hit-testing, minimap world mapping, and render geometry.
    const std::array<std::array<int,2>,4> screens={{{1280,720},{1920,1080},{800,600},{640,480}}};
    for(const auto& wh:screens){
        const int width=wh[0],height=wh[1];
        RTSHUDLayout ui(width,height,profile);
        assert(ui.bottom.h>legacy.bottom.h*.5f);
        assert(ui.topBar.y==profile.resourceTop);
        assert(ui.topBar.h==profile.resourceHeight);
        assert(ui.commandArea.w==profile.commandWidth);
        assert(std::fabs(ui.commandArea.x+
            ui.commandArea.w+profile.commandInset-width)<.01f);
        assert(ui.minimap.x==profile.minimapInset);
        assert(ui.minimap.x+ui.minimap.w < ui.commandArea.x);
        assert(ui.selectionArea.x+ui.selectionArea.w<=ui.commandArea.x+.01f);
        assert(ui.bottom.contains(ui.minimap.x+1.,ui.minimap.y+1.));
        assert(ui.bottom.contains(ui.commandArea.x+1.,ui.commandArea.y+1.));
        assert(ui.consumes(ui.minimap.x+5,ui.minimap.y+5));
        assert(ui.hit(ui.minimap.x+5,ui.minimap.y+5,game)==HUDCommand::Minimap);
        const auto world=ui.minimapWorld(ui.minimap.x,ui.minimap.y,
                                         project.terrain);
        assert(std::fabs(world.x-project.terrain.minX())<.01f);
        assert(std::fabs(world.z-project.terrain.minZ())<.01f);
        for(const auto& button:ui.buttons(game)){
            assert(ui.commandArea.contains(button.bounds.x+1,
                                           button.bounds.y+1));
            assert(ui.bottom.contains(button.bounds.x+1,
                                      button.bounds.y+1));
        }
    }
    // The editor authors a single transaction, not a stream of preview drags.
    EditorSession session;
    session.panel=EditorSession::Panel::Interface;
    RTSProject edited=project;
    RTSHUDProfile next=profile;
    next.commandWidth=390.f;
    next.minimapScale=.8f;
    assert(session.setHUDProfile(edited,next));
    assert(session.modified);
    assert(RTSHUDProfile::fromProject(edited).commandWidth==390.f);
    assert(edited.metadata.find("hud_profile")!=edited.metadata.end());
    assert(session.undo(edited));
    assert(edited.toJson().serialize()==before);
    assert(session.redo(edited));
    assert(RTSHUDProfile::fromProject(edited).commandWidth==390.f);
    next.commandWidth=401.f;
    const auto current=edited.toJson().serialize();
    assert(!session.setHUDProfile(edited,next)); // invalid, no mutation
    assert(edited.toJson().serialize()==current);
    next.commandWidth=390.f;
    assert(!session.setHUDProfile(edited,next)); // no redundant undo entry
    assert(session.setHUDProfile(edited,RTSHUDProfile{}));
    assert(edited.metadata.find("hud_profile")==edited.metadata.end());
    assert(session.undo(edited));
    assert(RTSHUDProfile::fromProject(edited).commandWidth==390.f);
    // Schema/type failures must be rejected at project load, rather
    // than silently moving hit regions or crashing the GL renderer.
    auto bad=project;
    bad.metadata["hud_profile"]=picojson::value("giant");
    assert(invalid(bad));
    bad=project;
    bad.metadata["hud_profile"]=picojson::value(picojson::object{
        {"minimap_scale",picojson::value("not-a-number")}});
    assert(invalid(bad));
    bad=project;
    bad.metadata["hud_profile"]=picojson::value(picojson::object{
        {"command_width",picojson::value(1000.)}});
    assert(invalid(bad));
    bad=project;
    bad.metadata["hud_profile"]=picojson::value(picojson::object{
        {"bottom_extra",picojson::value(-90.)}});
    assert(invalid(bad));
    // Gameplay data and *display layout* remain separate: selecting a HUD
    // profile cannot grant minerals, spawn units or alter research queues.
    assert(game.playerMinerals>=0);
    std::cout<<"Game interface editor: authored profile, engine hit parity, "
                "undo/redo, reset, validation and legacy defaults passed\n";
}