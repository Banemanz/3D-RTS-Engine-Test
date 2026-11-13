#include "InstanceOverrides.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main(){
    RTSProject p=RTSProject::defaults();
    auto& marine=p.placements[2];
    assert(marine.catalogId=="marine");
    marine.overrides["unit"]=picojson::value(picojson::object{
        {"damage",picojson::value(24.0)},{"max_hp",picojson::value(300.0)}
    });
    marine.overrides["actor"]=picojson::value(picojson::object{
        {"tint",picojson::value(picojson::array{
            picojson::value(.84),picojson::value(.19),picojson::value(.13)})},
        {"opacity",picojson::value(.5)}
    });
    marine.overrides["model"]=picojson::value(picojson::object{
        {"scale",picojson::value(picojson::array{
            picojson::value(1.2),picojson::value(2.0),picojson::value(.8)})},
        {"mesh",picojson::value("modded_placeholder_mesh")}
    });
    auto unique=resolvePlacedUnit(p,marine);
    assert(unique.unit.damage==24 && unique.unit.max_hp==300);
    assert(unique.actor.tint.x>.83f && unique.actor.opacity==.5f);
    assert(unique.actor.scale.y==2.f);
    assert(unique.actor.mesh=="modded_placeholder_mesh");
    assert(unitFromCatalog(p.catalog,"marine").damage==7);
    assert(resolvePlacedUnit(p,p.placements[3]).unit.damage==7);
    RTSProject restored=RTSProject::fromJson(p.toJson());
    assert(resolvePlacedUnit(restored,restored.placements[2]).unit.damage==24);
    restored.placements[2].overrides["actor"]=picojson::value("bad");
    bool bad=false;
    try{restored.validate();}catch(const std::invalid_argument&){bad=true;}
    assert(bad);
    std::cout<<"instance_overrides: isolated unit/actor/model fields and JSON save/load passed\n";
}
