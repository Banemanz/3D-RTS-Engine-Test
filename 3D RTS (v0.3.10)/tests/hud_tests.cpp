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
    assert(wide.hit(buttons[3].bounds.x+4,buttons[3].bounds.y+4,game)==HUDCommand::Build);
    assert(wide.hit(10,wide.bottom.y+5,game)==HUDCommand::Cancel);
    // No click on the top/bottom UI must turn into an RTS world order.
    for(const auto& button:buttons)
        assert(wide.consumes(button.bounds.x+1,button.bounds.y+1));
    game.supplyUsed=game.supplyCap;
    buttons=wide.buttons(game);
    assert(!buttons[5].enabled); // training is supply limited
    std::cout<<"rts-hud: hit targets, minimap mapping, abilities and panel boundaries passed\n";
}
