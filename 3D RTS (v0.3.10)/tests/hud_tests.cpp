#include "RTSHUDLayout.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    RTSSkirmish game;
    RTSHUDLayout wide(1280,720);
    RTSHUDLayout narrow(800,600);
    for(const auto& ui:{wide,narrow}) {
        assert(ui.bottom.y>ui.topBar.h);
        assert(ui.minimap.x+ui.minimap.w<ui.commandArea.x);
        assert(ui.selectionArea.x+ui.selectionArea.w<=ui.commandArea.x);
        assert(ui.consumes(12,12));
        assert(ui.consumes(12,ui.height-8));
        assert(!ui.consumes(ui.width*.5,ui.height*.5));
        assert(ui.hit(ui.minimap.x+5,ui.minimap.y+5,game)==HUDCommand::Minimap);
        auto world=ui.minimapWorld(ui.minimap.x,ui.minimap.y,game.project().terrain);
        assert(std::abs(world.x-game.project().terrain.minX())<.01f);
        assert(std::abs(world.z-game.project().terrain.minZ())<.01f);
    }
    auto buttons=wide.buttons(game);
    assert(buttons[0].command==HUDCommand::Move && !buttons[0].enabled);
    assert(buttons[6].command==HUDCommand::SelectArmy && buttons[6].enabled);
    for(auto& unit:game.units) {
        unit.isSelected=unit.ownerId==0 && unit.isAlive() && !unit.isStructure;
    }
    buttons=wide.buttons(game);
    assert(buttons[0].enabled && buttons[1].enabled && buttons[2].enabled);
    assert(buttons[3].enabled); // worker selected
    assert(wide.selectionCapacity()>=4);
    assert(narrow.selectionCapacity()>=2);
    std::vector<int> selectedIds;
    for(const auto& u:game.units)
        if(u.isAlive() && u.ownerId==0 && u.isSelected)selectedIds.push_back(u.id);
    assert(selectedIds.size()>1);
    for(std::size_t i=0;i<std::min(selectedIds.size(),
                                     std::size_t(wide.selectionCapacity()));++i){
        const auto card=wide.selectionCardBounds(int(i));
        assert(wide.selectionArea.contains(card.x+2,card.y+2));
        assert(wide.selectionUnitAt(card.x+4,card.y+4,game)==selectedIds[i]);
    }
    assert(wide.selectionUnitAt(4,4,game)==-1);
    assert(buttons[8].command==HUDCommand::Patrol && buttons[8].enabled);
    assert(wide.hit(buttons[3].bounds.x+4,buttons[3].bounds.y+4,game)==HUDCommand::Build);
    assert(wide.hit(10,wide.bottom.y+5,game)==HUDCommand::Cancel);
    // No click on the top/bottom UI must turn into an RTS world order.
    for(const auto& button:buttons)
        assert(wide.consumes(button.bounds.x+1,button.bounds.y+1));
    game.supplyUsed=game.supplyCap;
    buttons=wide.buttons(game);
    assert(!buttons[5].enabled); // training is supply limited
    // Gameplay UI must recognize data-driven replacement bases, not only
    // the original command_center catalog literal.
    RTSProject custom=RTSProject::defaults();
    assert(custom.catalog.cloneDerived("command_center","custom_hq"));
    custom.metadata["player_base"]=picojson::value("custom_hq");
    custom.placements[0].catalogId="custom_hq";
    game.loadProject(custom);
    auto customBtns=wide.buttons(game);
    assert(customBtns[7].enabled);
    for(auto& u:game.units)u.isSelected=(u.id==game.playerBaseId());
    customBtns=wide.buttons(game);
    const auto card=wide.selectionCardBounds(0);
    assert(wide.selectionUnitAt(card.x+2,card.y+2,game)==-1);
    assert(customBtns[8].command==HUDCommand::CancelQueue && !customBtns[8].enabled);
    assert(game.queueWorker());
    customBtns=wide.buttons(game);
    assert(customBtns[8].enabled);
    assert(wide.hit(customBtns[8].bounds.x+3,customBtns[8].bounds.y+3,game)==HUDCommand::CancelQueue);
    bool found=false;
    for(const auto& u:game.units)if(u.id==game.playerBaseId()){
        assert(u.catalogId=="custom_hq");
        found=true;
    }
    assert(found);
    std::cout<<"rts-hud: hit targets, minimap mapping, abilities and panel boundaries passed\n";
}
