#include "RTSProject.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
const picojson::value& required(const picojson::object& o,const std::string& key) {
    auto it=o.find(key);
    if(it==o.end())throw std::invalid_argument("Missing project property: "+key);
    return it->second;
}
int integer(const picojson::value& v,const std::string& field,int min,int max) {
    if(!v.is<double>())throw std::invalid_argument(field+" must be integer");
    double d=v.get<double>();
    if(!std::isfinite(d) || std::floor(d)!=d || d<min || d>max)
        throw std::invalid_argument(field+" outside allowed range");
    return static_cast<int>(d);
}
double number(const picojson::value& v,const std::string& field,double min,double max) {
    if(!v.is<double>())throw std::invalid_argument(field+" must be number");
    double d=v.get<double>();
    if(!std::isfinite(d)||d<min||d>max)
        throw std::invalid_argument(field+" outside allowed range");
    return d;
}
std::string string(const picojson::value& v,const std::string& field) {
    if(!v.is<std::string>())throw std::invalid_argument(field+" must be string");
    return v.get<std::string>();
}
picojson::value encodeVec(Vec3 p) {
    return picojson::value(picojson::array{
        picojson::value(double(p.x)),picojson::value(double(p.y)),picojson::value(double(p.z))});
}
Vec3 decodeVec(const picojson::value& v,const std::string& field) {
    if(!v.is<picojson::array>() || v.get<picojson::array>().size()!=3)
        throw std::invalid_argument(field+" must be [x,y,z]");
    const auto& a=v.get<picojson::array>();
    return Vec3(float(number(a[0],field,-100000,100000)),
                float(number(a[1],field,-100000,100000)),
                float(number(a[2],field,-100000,100000)));
}
picojson::object object(const picojson::value& v,const std::string& field) {
    if(!v.is<picojson::object>())throw std::invalid_argument(field+" must be object");
    return v.get<picojson::object>();
}
ProjectPlacement makePlacement(std::string id,std::string kind,std::string catalog,
                               std::string name,int owner,float x,float z,int amount=0) {
    ProjectPlacement p;p.id=std::move(id);p.kind=std::move(kind);
    p.catalogId=std::move(catalog);p.name=std::move(name);
    p.owner=owner;p.position={x,0,z};p.amount=amount;return p;
}
}
RTSProject RTSProject::defaults() {
    RTSProject p;
    p.name="First Contact";
    p.description="Starting skirmish and editable sandbox project";
    p.catalog=DataCatalog::defaults();
    p.placements.push_back(makePlacement("human_base","Unit","command_center","Command Center",0,-22,0));
    p.placements.push_back(makePlacement("enemy_base","Unit","hive","Hybrid Hive",1,22,0));
    for(int i=0;i<4;++i)
        p.placements.push_back(makePlacement("marine_"+std::to_string(i+1),"Unit","marine",
                           "Marine "+std::to_string(i+1),0,-13,-4.5f+i*3.0f));
    for(int i=0;i<2;++i)
        p.placements.push_back(makePlacement("worker_"+std::to_string(i+1),"Unit","worker",
                           "Worker "+std::to_string(i+1),0,-18,-5.5f+i*11.f));
    for(int i=0;i<2;++i)
        p.placements.push_back(makePlacement("zealot_"+std::to_string(i+1),"Unit","zealot",
                           "Zealot "+std::to_string(i+1),1,13,-3.5f+i*7.f));
    p.placements.push_back(makePlacement("mineral_north","Mineral","","Minerals North",-1,-3,-12,750));
    p.placements.push_back(makePlacement("mineral_south","Mineral","","Minerals South",-1,-3,12,750));
    p.placements.push_back(makePlacement("mineral_center","Mineral","","Minerals Center",-1,0,0,1000));
    p.metadata["player_starting_minerals"]=picojson::value(220.0);
    p.metadata["player_supply_cap"]=picojson::value(18.0);
    p.metadata["enemy_spawn_interval"]=picojson::value(16.0);
    return p;
}
picojson::value RTSProject::toJson() const {
    validate();
    picojson::object root=extensions;
    root["schema_version"]=picojson::value(double(schemaVersion));
    root["name"]=picojson::value(name);
    root["description"]=picojson::value(description);
    root["catalog"]=catalog.serialize();
    root["metadata"]=picojson::value(metadata);
    picojson::object m;
    m["width"]=picojson::value(double(terrain.width()));
    m["depth"]=picojson::value(double(terrain.depth()));
    m["spacing"]=picojson::value(double(terrain.spacing()));
    m["min_x"]=picojson::value(double(terrain.minX()));
    m["min_z"]=picojson::value(double(terrain.minZ()));
    picojson::array cells;
    for(int z=0;z<terrain.depth();++z)for(int x=0;x<terrain.width();++x) {
        const auto& c=terrain.cell(x,z);
        if(c.height==0 && c.material==0 && !c.blocked)continue;
        picojson::object v;
        v["x"]=picojson::value(double(x));
        v["z"]=picojson::value(double(z));
        if(c.height!=0)v["height"]=picojson::value(double(c.height));
        if(c.material!=0)v["material"]=picojson::value(double(c.material));
        if(c.blocked)v["blocked"]=picojson::value(true);
        cells.emplace_back(v);
    }
    m["cells"]=picojson::value(cells);
    root["terrain"]=picojson::value(m);
    picojson::array entities;
    for(const auto& p:placements) {
        picojson::object v{
            {"id",picojson::value(p.id)},{"kind",picojson::value(p.kind)},
            {"catalog",picojson::value(p.catalogId)},{"name",picojson::value(p.name)},
            {"owner",picojson::value(double(p.owner))},{"position",encodeVec(p.position)},
            {"rotation",encodeVec(p.rotation)},{"scale",encodeVec(p.scale)},
            {"amount",picojson::value(double(p.amount))},
            {"overrides",picojson::value(p.overrides)}
        };
        entities.emplace_back(v);
    }
    root["placements"]=picojson::value(entities);
    return picojson::value(root);
}
RTSProject RTSProject::fromJson(const picojson::value& rootValue) {
    if(!rootValue.is<picojson::object>())throw std::invalid_argument("Project root must be object");
    const auto& root=rootValue.get<picojson::object>();
    RTSProject result;
    result.schemaVersion=integer(required(root,"schema_version"),"schema_version",1,1);
    result.name=string(required(root,"name"),"name");
    if(root.count("description"))result.description=string(root.at("description"),"description");
    result.catalog=DataCatalog::deserialize(required(root,"catalog"));
    if(root.count("metadata"))result.metadata=object(root.at("metadata"),"metadata");
    const auto t=object(required(root,"terrain"),"terrain");
    int width=integer(required(t,"width"),"width",2,257);
    int depth=integer(required(t,"depth"),"depth",2,257);
    float spacing=float(number(required(t,"spacing"),"spacing",0.25,64.0));
    float minX=float(number(required(t,"min_x"),"min_x",-50000,50000));
    float minZ=float(number(required(t,"min_z"),"min_z",-50000,50000));
    result.terrain=TerrainMap(width,depth,spacing,minX,minZ);
    if(t.count("cells")) {
        if(!t.at("cells").is<picojson::array>())
            throw std::invalid_argument("Terrain cells must be array");
        const auto& cells=t.at("cells").get<picojson::array>();
        if(cells.size()>static_cast<std::size_t>(width*depth))
            throw std::invalid_argument("Too many terrain cell overrides");
        std::vector<bool> seen(static_cast<std::size_t>(width*depth),false);
        for(const auto& element:cells) {
            const auto v=object(element,"terrain cell");
            int x=integer(required(v,"x"),"cell.x",0,width-1);
            int z=integer(required(v,"z"),"cell.z",0,depth-1);
            std::size_t idx=static_cast<std::size_t>(z*width+x);
            if(seen[idx])throw std::invalid_argument("Duplicate terrain cell override");
            seen[idx]=true;
            TerrainMap::Cell c;
            if(v.count("height"))c.height=float(number(v.at("height"),"height",-60,60));
            if(v.count("material"))
                c.material=static_cast<unsigned char>(integer(v.at("material"),"material",0,7));
            if(v.count("blocked")) {
                if(!v.at("blocked").is<bool>())throw std::invalid_argument("blocked must be boolean");
                c.blocked=v.at("blocked").get<bool>();
            }
            if(!result.terrain.setCell(x,z,c))throw std::invalid_argument("Bad terrain cell");
        }
    }
    const auto& entities=required(root,"placements");
    if(!entities.is<picojson::array>() ||
       entities.get<picojson::array>().size()>20000)
        throw std::invalid_argument("Invalid placements list");
    for(const auto& element:entities.get<picojson::array>()) {
        const auto v=object(element,"placement");
        ProjectPlacement p;
        p.id=string(required(v,"id"),"placement.id");
        p.kind=string(required(v,"kind"),"placement.kind");
        if(v.count("catalog"))p.catalogId=string(v.at("catalog"),"catalog");
        if(v.count("name"))p.name=string(v.at("name"),"name");
        p.owner=v.count("owner")?integer(v.at("owner"),"owner",-1,15):0;
        p.position=decodeVec(required(v,"position"),"position");
        if(v.count("rotation"))p.rotation=decodeVec(v.at("rotation"),"rotation");
        if(v.count("scale"))p.scale=decodeVec(v.at("scale"),"scale");
        if(v.count("amount"))p.amount=integer(v.at("amount"),"amount",0,10000000);
        if(v.count("overrides"))p.overrides=object(v.at("overrides"),"overrides");
        result.placements.push_back(std::move(p));
    }
    for(const auto& [key,value]:root)
        if(key!="schema_version" && key!="name" && key!="description" &&
           key!="catalog" && key!="metadata" && key!="terrain" && key!="placements")
            result.extensions[key]=value;
    result.validate();
    result.terrain.clearHistory();
    result.terrain.clearDirty();
    return result;
}
void RTSProject::validate() const {
    if(schemaVersion!=1 || name.empty() || name.size()>256 || !terrain.valid())
        throw std::invalid_argument("Invalid project name/version/terrain");
    catalog.validate();
    std::set<std::string> ids;
    for(const auto& p:placements) {
        if(p.id.empty() || !ids.insert(p.id).second || p.id.size()>128)
            throw std::invalid_argument("Missing/duplicate placement ID");
        if(p.scale.x<=0 || p.scale.y<=0 || p.scale.z<=0)
            throw std::invalid_argument("Nonpositive placement scale: "+p.id);
        if(!terrain.insideWorld(p.position.x,p.position.z))
            throw std::invalid_argument("Placement outside terrain map: "+p.id);
        if(p.kind=="Unit") {
            const auto* e=catalog.find(p.catalogId);
            if(!e || e->type!="Unit")
                throw std::invalid_argument("Missing Unit catalog for: "+p.id);
        } else if(p.kind=="Doodad") {
            const auto* e=catalog.find(p.catalogId);
            if(!e || (e->type!="Actor" && e->type!="Model"))
                throw std::invalid_argument("Missing actor/model for doodad: "+p.id);
        } else if(p.kind!="Mineral" && p.kind!="Point" && p.kind!="Region")
            throw std::invalid_argument("Unsupported placement kind: "+p.kind);
    }
}
void RTSProject::loadFile(const std::string& filename) {
    std::ifstream stream(filename,std::ios::binary);
    if(!stream)throw std::runtime_error("Could not open project: "+filename);
    stream.seekg(0,std::ios::end);
    auto length=stream.tellg();
    if(length<0 || length>16*1024*1024)
        throw std::runtime_error("Project file too large or unreadable");
    stream.seekg(0,std::ios::beg);
    std::string data(static_cast<std::size_t>(length),'\0');
    if(!data.empty())stream.read(&data[0],length);
    if(!stream && !data.empty())throw std::runtime_error("Failed reading project");
    picojson::value parsed;
    const std::string error=picojson::parse(parsed,data);
    if(!error.empty())throw std::invalid_argument("Project JSON: "+error);
    *this=fromJson(parsed);
}
void RTSProject::saveFile(const std::string& filename) const {
    std::filesystem::path path(filename);
    if(!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    const std::string tmp=filename+".tmp";
    {
        std::ofstream stream(tmp,std::ios::binary|std::ios::trunc);
        if(!stream)throw std::runtime_error("Cannot save project to: "+tmp);
        stream<<toJson().serialize(true)<<"\n";
        stream.flush();
        if(!stream)throw std::runtime_error("Project write failed: "+tmp);
    }
#if defined(_WIN32)
    if(!MoveFileExA(tmp.c_str(),filename.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Replacing project file failed: "+filename);
#else
    if(std::rename(tmp.c_str(),filename.c_str())!=0)
        throw std::runtime_error("Replacing project file failed: "+filename);
#endif
}
