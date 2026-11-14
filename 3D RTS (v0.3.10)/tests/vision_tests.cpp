#include "RTSVision.hpp"
#include "RTSSkirmish.hpp"
#include <cassert>
#include <iostream>
#include <vector>

int main(){
    TerrainMap map(12,12,2.f,0.f,0.f);
    UnitData scout;
    scout.sight_range=23.f;
    Unit unit(1,"Scout",scout,{2,0,12});
    unit.ownerId=0;
    std::vector<Unit> units{unit};
    RTSVision vision;
    vision.reset(map);
    assert(vision.width()==12 && vision.depth()==12);
    assert(!vision.visible({2,0,12}));
    const auto before=vision.revision();
    vision.update(map,units);
    assert(vision.revision()>before);
    assert(vision.visible({2,0,12}));
    assert(vision.visible({18,0,12}));
    assert(!vision.visible({500,0,500}));
    const auto initial=vision.revision();
    vision.update(map,units);
    assert(vision.revision()==initial); // unchanged sight shouldn't retrigger GPU upload

    // A heightfield crest is not transparent to ground-level sensors.
    map.setCell(5,6,{12.f,2,false});
    vision.reset(map);
    vision.update(map,units);
    assert(vision.cell(5,6)==RTSVision::State::Visible || vision.cell(5,6)==RTSVision::State::Unexplored);
    assert(!vision.visible({18,0,12}));
    map.setCell(5,6,{0.f,2,false});
    vision.update(map,units);
    assert(vision.visible({18,0,12}));
    // Losing a sensor preserves explored cells, but not current visibility.
    units[0].currentHp=0.f;
    vision.update(map,units);
    assert(vision.atWorld({18,0,12})==RTSVision::State::Explored);
    assert(vision.textureBytes().size()==12u*12u);
    assert(vision.textureBytes()[6*12+9]==96);
    vision.reset(map);
    assert(vision.atWorld({18,0,12})==RTSVision::State::Unexplored);

    RTSSkirmish game;
    const Unit* enemy=nullptr;
    for(const auto& u:game.units)if(u.ownerId==1 && u.isStructure){enemy=&u;break;}
    assert(enemy!=nullptr);
    assert(!game.vision.visible(enemy->position));
    const auto* ranger=game.project().catalog.find("ranger");
    assert(ranger!=nullptr);
    assert(unitFromCatalog(game.project().catalog,"ranger").sight_range==14.f);
    RTSProject custom=RTSProject::defaults();
    assert(custom.catalog.setField("ranger","sight_range",picojson::value(3.0)));
    game.loadProject(custom);
    assert(unitFromCatalog(game.project().catalog,"ranger").sight_range==3.f);
    std::cout<<"vision: exploration, terrain LOS, fog texture, catalog sight range passed\n";
}
