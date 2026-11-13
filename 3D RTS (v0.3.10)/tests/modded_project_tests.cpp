#include "RTSSkirmish.hpp"
#include <cassert>
#include <iostream>

int main(){
    RTSProject p=RTSProject::defaults();
    assert(p.catalog.setField("marine","max_hp",picojson::value(100.0)));
    assert(p.catalog.setField("marine_actor","tint",picojson::value(
        picojson::array{picojson::value(.7),picojson::value(.2),picojson::value(.3)})));
    p.placements.erase(p.placements.begin()+5);
    ProjectPlacement extra;
    extra.id="editor_added_unit";extra.kind="Unit";extra.catalogId="marine";
    extra.name="Editor Custom";extra.owner=0;extra.position={-8,0,7};
    extra.overrides["unit"]=picojson::value(picojson::object{
        {"max_hp",picojson::value(155.0)},{"damage",picojson::value(25.0)}});
    extra.overrides["actor"]=picojson::value(picojson::object{
        {"opacity",picojson::value(.65)},{"tint",picojson::value(picojson::array{
            picojson::value(.9),picojson::value(.3),picojson::value(.15)})}});
    extra.overrides["model"]=picojson::value(picojson::object{
        {"scale",picojson::value(picojson::array{
            picojson::value(1.8),picojson::value(1.2),picojson::value(.8)})}});
    extra.scale={1.2f,1.f,1.f};
    p.placements.push_back(extra);
    p.terrain.setCell(32,30,{4.0f,2,true});
    p.validate();
    RTSSkirmish game;
    game.loadProject(p);
    bool found=false,painted=false;
    for(const auto& unit:game.units)if(unit.instanceName=="Editor Custom"){
        found=true;
        assert(unit.currentHp==155.0f);
        assert(unit.data.damage==25.0f);
        assert(unit.data.faction_color.x>.89f && unit.data.faction_color.x<.91f);
        assert(unit.actorOpacity>.64f && unit.actorOpacity<.66f);
        assert(unit.modelScale.x>2.15f && unit.modelScale.x<2.17f);
    }
    for(const auto& u:game.units)if(u.catalogId=="marine" && u.data.max_hp==100.f)
        painted=true;
    assert(found && painted);
    assert(!game.navigation.walkable(32,30));
    p.terrain.setCell(32,30,{1.5f,3,false});
    game.syncTerrain(p.terrain);
    assert(game.navigation.walkable(32,30));
    p.terrain.setCell(10,10,{10.f,0,false});
    p.metadata["max_walkable_slope"]=picojson::value(1.0);
    game.loadProject(p);
    assert(!game.navigation.walkable(10,10));
    p.terrain.setCell(10,10,{0.f,0,false});
    game.syncTerrain(p.terrain);
    assert(game.navigation.walkable(10,10));
    std::cout<<"modded_project: catalog edits, scene placements and path mask apply\n";
}
