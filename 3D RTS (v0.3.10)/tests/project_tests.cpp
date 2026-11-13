#include "RTSProject.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
    RTSProject p=RTSProject::defaults();
    assert(p.placements.size()==13);
    assert(p.catalog.find("marine") && p.catalog.find("marine_actor"));
    assert(p.catalog.setField("marine","damage",picojson::value(31.0)));
    p.terrain.beginStroke();
    assert(p.terrain.applyBrush({0,0,0},TerrainMap::Brush::Raise,5,4));
    assert(p.terrain.applyBrush({0,0,0},TerrainMap::Brush::Paint,5,1,2));
    assert(p.terrain.applyBrush({0,0,0},TerrainMap::Brush::Block,5,1));
    p.terrain.endStroke();
    p.extensions["external_tool_metadata"]=picojson::value(picojson::array{
        picojson::value("preserve me"),picojson::value(42.0)});
    const auto path=std::filesystem::temp_directory_path()/"rts-project-test.json";
    p.saveFile(path.string());
    RTSProject loaded;loaded.loadFile(path.string());
    assert(loaded.name=="First Contact" && loaded.placements.size()==13);
    assert(loaded.terrain.cell(24,24).height>3.9f);
    assert(loaded.terrain.cell(24,24).material==2);
    assert(loaded.terrain.cell(24,24).blocked);
    assert(unitFromCatalog(loaded.catalog,"marine").damage==31.0f);
    assert(loaded.extensions.count("external_tool_metadata")==1);
    std::filesystem::remove(path);
    picojson::value json=p.toJson();
    auto& root=json.get<picojson::object>();
    root["schema_version"]=picojson::value(99.0);
    bool error=false;
    try{auto unused=RTSProject::fromJson(json);(void)unused;}
    catch(const std::invalid_argument&){error=true;}
    assert(error);
    std::cout<<"project: JSON save/load, data links, sparse terrain, extension fields passed\n";
}
