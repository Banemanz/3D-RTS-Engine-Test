#include "RTSProject.hpp"
#include "InstanceOverrides.hpp"
#include "TriggerDefinition.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <set>
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

// Walk only typed ECA nodes. The same visitor handles mutable rename
// migration and const inbound-reference inspection, including nested branches.
template<class RuleObject,class Visitor>
void visitECARule(RuleObject& rule,Visitor visitor){
    const auto descend=[&](auto&& self,auto& value,const char* section,int depth)->void{
        if(depth>6 || !value.template is<picojson::object>())return;
        auto& node=value.template get<picojson::object>();
        visitor(node,section);
        auto kind=node.find("type");
        if(kind==node.end() || !kind->second.template is<std::string>())return;
        const auto& type=kind->second.template get<std::string>();
        const auto nested=[&](const char* key,const char* nestedSection){
            auto children=node.find(key);
            if(children==node.end() || !children->second.template is<picojson::array>())
                return;
            for(auto& child:children->second.template get<picojson::array>())
                self(self,child,nestedSection,depth+1);
        };
        if(type=="IfThenElse"){
            nested("conditions","conditions");
            nested("then","actions");
            nested("else","actions");
        }else if(type=="Repeat")nested("actions","actions");
    };
    for(const char* section:{"conditions","actions"}){
        auto group=rule.find(section);
        if(group==rule.end() || !group->second.template is<picojson::array>())continue;
        for(auto& item:group->second.template get<picojson::array>())
            descend(descend,item,section,0);
    }
}
}
RTSProject RTSProject::defaults() {
    RTSProject p;
    p.name="First Contact";
    p.description="Starting skirmish and editable sandbox project";
    p.catalog=DataCatalog::defaults();
    p.placements.push_back(makePlacement("human_base","Unit","command_center","Command Center",0,-22,0));
    p.placements.push_back(makePlacement("enemy_base","Unit","rift_bastion","Hybrid Bastion",1,22,0));
    for(int i=0;i<4;++i)
        p.placements.push_back(makePlacement("ranger_"+std::to_string(i+1),"Unit","ranger",
                           "Ranger "+std::to_string(i+1),0,-13,-4.5f+i*3.0f));
    for(int i=0;i<2;++i)
        p.placements.push_back(makePlacement("worker_"+std::to_string(i+1),"Unit","worker",
                           "Worker "+std::to_string(i+1),0,-18,-5.5f+i*11.f));
    for(int i=0;i<2;++i)
        p.placements.push_back(makePlacement("rift_raider_"+std::to_string(i+1),"Unit","rift_raider",
                           "Raider "+std::to_string(i+1),1,13,-3.5f+i*7.f));
    p.placements.push_back(makePlacement("mineral_north","Mineral","","Minerals North",-1,-3,-12,750));
    p.placements.push_back(makePlacement("mineral_south","Mineral","","Minerals South",-1,-3,12,750));
    p.placements.push_back(makePlacement("mineral_center","Mineral","","Minerals Center",-1,0,0,1000));
    p.metadata["player_ranger"]=picojson::value("ranger");
    p.metadata["player_starting_minerals"]=picojson::value(220.0);
    p.metadata["player_supply_cap"]=picojson::value(18.0);
    p.metadata["enemy_spawn_interval"]=picojson::value(16.0);
    p.metadata["formation_spacing_multiplier"]=picojson::value(2.4);
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
    // Author-facing AI parameters are validated before saving/replaying;
    // malformed metadata must not silently change AI policy.
    const auto aiEnabled=metadata.find("enemy_ai_enabled");
    if(aiEnabled!=metadata.end() && !aiEnabled->second.is<bool>())
        throw std::invalid_argument("enemy_ai_enabled must be boolean");
    const auto guardRadius=metadata.find("enemy_ai_guard_radius");
    if(guardRadius!=metadata.end() &&
       (!guardRadius->second.is<double>() ||
        !std::isfinite(guardRadius->second.get<double>()) ||
        guardRadius->second.get<double>()<0. ||
        guardRadius->second.get<double>()>80.))
        throw std::invalid_argument("enemy_ai_guard_radius must be between 0 and 80");
    // Any game-facing data object must be valid before we allow project saves.
    for(const auto& e:catalog.entries()) {
        if(e.type=="Unit") {
            (void)unitFromCatalog(catalog,e.id);
            (void)unitAbilityIds(catalog,e.id);
            const auto fields=catalog.resolvedFields(e.id);
            const auto trains=fields.find("trainable_units");
            if(trains!=fields.end()) {
                if(!trains->second.is<picojson::array>() ||
                   trains->second.get<picojson::array>().size()>12)
                    throw std::invalid_argument(e.id+": trainable_units must be an array of <=12 IDs");
                std::set<std::string> unique;
                for(const auto& choice:trains->second.get<picojson::array>()) {
                    if(!choice.is<std::string>())
                        throw std::invalid_argument(e.id+": training entries must be unit IDs");
                    const std::string& id=choice.get<std::string>();
                    const auto* target=catalog.find(id);
                    if(!target || target->type!="Unit" || !unique.insert(id).second)
                        throw std::invalid_argument(e.id+": missing/duplicate trainable Unit "+id);
                    const auto targetFields=catalog.resolvedFields(id);
                    auto structure=targetFields.find("is_structure");
                    if(structure!=targetFields.end() &&
                       (!structure->second.is<bool>() || structure->second.get<bool>()))
                        throw std::invalid_argument(e.id+": cannot train structures: "+id);
                }
            }
        } else if(e.type=="Actor")(void)actorFromCatalog(catalog,e.id);
    }
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
            (void)resolvePlacedUnit(*this,p); // validates known instance overrides
        } else if(p.kind=="Doodad") {
            const auto* e=catalog.find(p.catalogId);
            if(!e || (e->type!="Actor" && e->type!="Model"))
                throw std::invalid_argument("Missing actor/model for doodad: "+p.id);
            (void)resolvePlacedDoodad(*this,p);
        } else if(p.kind!="Mineral" && p.kind!="Point" && p.kind!="Region")
            throw std::invalid_argument("Unsupported placement kind: "+p.kind);
    }
}
bool RTSProject::renameCatalogObject(const std::string& oldId,const std::string& newId) {
    const auto* source=catalog.find(oldId);
    if(!source || oldId==newId)return false;
    const std::string type=source->type;
    RTSProject next=*this;
    if(!next.catalog.rename(oldId,newId))return false;
    for(auto& placement:next.placements) {
        if(placement.catalogId==oldId)placement.catalogId=newId;
        // Preserve unrecognized mod fields and strings; only typed references migrate.
        const char* section=type=="Actor"?"unit":
             type=="Model"?"actor":type=="Requirement"?"unit":nullptr;
        const char* property=type=="Actor"?"actor":
             type=="Model"?"model":type=="Requirement"?"requirement":nullptr;
        if(!section)continue;
        auto category=placement.overrides.find(section);
        if(category==placement.overrides.end() || !category->second.is<picojson::object>())continue;
        auto& overrides=category->second.get<picojson::object>();
        auto field=overrides.find(property);
        if(field!=overrides.end() && field->second.is<std::string>() &&
           field->second.get<std::string>()==oldId)
            field->second=picojson::value(newId);
    }
    if(type=="Unit") {
        // The built-in skirmish has six metadata-configurable unit roles.
        // Supply explicit mappings when an implicit fallback is renamed.
        const std::pair<const char*,const char*> roles[]={
            {"player_ranger","ranger"},{"player_marine","marine"},
            {"player_worker","worker"},{"enemy_soldier","rift_raider"},
            {"enemy_soldier","zealot"},{"player_barracks","barracks"},
            {"player_base","command_center"},{"enemy_base","rift_bastion"},
            {"enemy_base","hive"}};
        for(const auto& role:roles) {
            auto field=next.metadata.find(role.first);
            if(field==next.metadata.end()) {
                if(oldId==role.second)next.metadata[role.first]=picojson::value(newId);
            }else if(field->second.is<std::string>() &&
                     field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
    }
    if(type=="Unit" || type=="Effect"){
        auto triggers=next.extensions.find("triggers");
        if(triggers!=next.extensions.end() && triggers->second.is<picojson::array>())
            for(auto& value:triggers->second.get<picojson::array>()){
                if(!value.is<picojson::object>())continue;
                auto& rule=value.get<picojson::object>();
                visitECARule(rule,[&](picojson::object& node,const char* section){
                    const auto kind=node.find("type");
                    if(kind==node.end() || !kind->second.is<std::string>())return;
                    const std::string& op=kind->second.get<std::string>();
                    const bool unitRef=type=="Unit" &&
                        ((std::string(section)=="actions" && op=="SpawnUnit") ||
                         (std::string(section)=="conditions" &&
                          (op=="UnitTypeCountAtLeast" || op=="UnitCountAtLeast")));
                    const bool effectRef=type=="Effect" &&
                        std::string(section)=="actions" && op=="ApplyEffect";
                    if(!unitRef && !effectRef)return;
                    const char* field=unitRef?"catalog":"effect";
                    auto ref=node.find(field);
                    if(ref!=node.end() && ref->second.is<std::string>() &&
                       ref->second.get<std::string>()==oldId)
                        ref->second=picojson::value(newId);
                });
            }
    }
    next.validate();
    (void)parseTriggerRules(next); // typed ECA links must also remain valid
    *this=std::move(next);
    return true;
}
std::vector<std::string> RTSProject::catalogReferences(const std::string& id) const {
    std::vector<std::string> refs;
    const auto* original=catalog.find(id);
    if(!original)return refs;
    for(const auto& entry:catalog.entries())if(entry.id!=id) {
        if(entry.parent==id)refs.push_back("inherited by "+entry.id);
        const auto properties=catalog.resolvedFields(entry.id);
        const char* typed=(original->type=="Actor" && entry.type=="Unit")?"actor":
            (original->type=="Model" && entry.type=="Actor")?"model":nullptr;
        if(typed) {
            auto it=properties.find(typed);
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+"."+typed);
        }
        if(original->type=="Ability" && entry.type=="Unit"){
            const auto abilityLinks=properties.find("abilities");
            if(abilityLinks!=properties.end() && abilityLinks->second.is<picojson::array>())
                for(const auto& ref:abilityLinks->second.get<picojson::array>())
                    if(ref.is<std::string>() && ref.get<std::string>()==id)
                        refs.push_back(entry.id+".abilities");
        }
        if(original->type=="Weapon" && entry.type=="Unit"){
            const auto linked=properties.find("weapon");
            if(linked!=properties.end() && linked->second.is<std::string>() &&
               linked->second.get<std::string>()==id)
                refs.push_back(entry.id+".weapon");
        }
        if(original->type=="Projectile" &&
           (entry.type=="Ability" || entry.type=="Weapon")){
            const auto linked=properties.find("projectile");
            if(linked!=properties.end() && linked->second.is<std::string>() &&
               linked->second.get<std::string>()==id)
                refs.push_back(entry.id+".projectile");
        }

        if(original->type=="Requirement" &&
           (entry.type=="Ability" || entry.type=="Unit")){
            const auto it=properties.find("requirement");
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+".requirement");
        }
        if(original->type=="Requirement" && entry.type=="Requirement"){
            const auto it=properties.find("children");
            if(it!=properties.end() && it->second.is<picojson::array>())
                for(const auto& value:it->second.get<picojson::array>())
                    if(value.is<std::string>() && value.get<std::string>()==id)
                        refs.push_back(entry.id+".children");
        }
        if(original->type=="Upgrade" && entry.type=="Unit"){
            const auto it=properties.find("researchable_upgrades");
            if(it!=properties.end() && it->second.is<picojson::array>())
                for(const auto& value:it->second.get<picojson::array>())
                    if(value.is<std::string>() && value.get<std::string>()==id)
                        refs.push_back(entry.id+".researchable_upgrades");
        }
        if(original->type=="Upgrade" && entry.type=="Validator"){
            const auto it=properties.find("upgrade");
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+".upgrade");
        }
        if(original->type=="Unit" && entry.type=="Upgrade"){
            const auto it=properties.find("unit");
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+".unit");
        }
        if(original->type=="Requirement" && entry.type=="Upgrade"){
            const auto it=properties.find("requirement");
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+".requirement");
        }
        if(original->type=="Validator" && entry.type=="Requirement"){
            const auto it=properties.find("validator");
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+".validator");
        }
        if(original->type=="Behavior" && entry.type=="Validator"){
            const auto it=properties.find("behavior");
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+".behavior");
        }
        if(original->type=="Unit" && entry.type=="Validator"){
            const auto it=properties.find("unit");
            if(it!=properties.end() && it->second.is<std::string>() &&
               it->second.get<std::string>()==id)
                refs.push_back(entry.id+".unit");
        }
        if(original->type=="Behavior" && entry.type=="Effect"){
            const auto linked=properties.find("behavior");
            if(linked!=properties.end() && linked->second.is<std::string>() &&
               linked->second.get<std::string>()==id)
                refs.push_back(entry.id+".behavior");
        }
        if(original->type=="Effect" && entry.type=="Ability"){
            const auto effect=properties.find("effect");
            if(effect!=properties.end() && effect->second.is<std::string>() &&
               effect->second.get<std::string>()==id)
                refs.push_back(entry.id+".effect");
        }
        if(original->type=="Effect" && entry.type=="Effect"){
            auto it=properties.find("children");
            if(it!=properties.end() && it->second.is<picojson::array>())
                for(const auto& child:it->second.get<picojson::array>())
                    if(child.is<std::string>() && child.get<std::string>()==id)
                        refs.push_back(entry.id+".children");
        }
        if(original->type=="Unit" && entry.type=="Unit") {
            auto it=properties.find("trainable_units");
            if(it!=properties.end() && it->second.is<picojson::array>())
                for(const auto& entryId:it->second.get<picojson::array>())
                    if(entryId.is<std::string>() && entryId.get<std::string>()==id)
                        refs.push_back(entry.id+".trainable_units");
        }
    }
    for(const auto& placement:placements) {
        if(placement.catalogId==id)refs.push_back("placement "+placement.id);
        const char* section=original->type=="Actor"?"unit":
            original->type=="Model"?"actor":
            original->type=="Requirement"?"unit":nullptr;
        const char* property=original->type=="Actor"?"actor":
            original->type=="Model"?"model":
            original->type=="Requirement"?"requirement":nullptr;
        if(!section)continue;
        auto category=placement.overrides.find(section);
        if(category==placement.overrides.end() ||
           !category->second.is<picojson::object>())continue;
        const auto& fields=category->second.get<picojson::object>();
        auto it=fields.find(property);
        if(it!=fields.end() && it->second.is<std::string>() &&
           it->second.get<std::string>()==id)
            refs.push_back("placement "+placement.id+"."+section+"."+property);
    }
    if(original->type=="Unit") {
        const std::pair<const char*,const char*> roles[]={
            {"player_ranger","ranger"},{"player_marine","marine"},
            {"player_worker","worker"},{"enemy_soldier","rift_raider"},
            {"enemy_soldier","zealot"},{"player_barracks","barracks"},
            {"player_base","command_center"},{"enemy_base","rift_bastion"},
            {"enemy_base","hive"}};
        for(const auto& role:roles) {
            auto it=metadata.find(role.first);
            if((it==metadata.end() && id==role.second) ||
               (it!=metadata.end() && it->second.is<std::string>() &&
                it->second.get<std::string>()==id))
                refs.push_back(std::string("skirmish role ")+role.first);
        }
    }
    if(original->type=="Unit" || original->type=="Effect"){
        auto trigger=extensions.find("triggers");
        if(trigger!=extensions.end() && trigger->second.is<picojson::array>())
            for(const auto& value:trigger->second.get<picojson::array>()){
                if(!value.is<picojson::object>())continue;
                const auto& rule=value.get<picojson::object>();
                const auto name=rule.find("id");
                const std::string ruleName=name!=rule.end() &&
                    name->second.is<std::string>()?
                    name->second.get<std::string>():"<unnamed>";
                visitECARule(rule,[&](const picojson::object& node,const char* section){
                    const auto kind=node.find("type");
                    if(kind==node.end() || !kind->second.is<std::string>())return;
                    const std::string& op=kind->second.get<std::string>();
                    const bool unitRef=original->type=="Unit" &&
                        ((std::string(section)=="actions" && op=="SpawnUnit") ||
                         (std::string(section)=="conditions" &&
                          (op=="UnitTypeCountAtLeast" || op=="UnitCountAtLeast")));
                    const bool effectRef=original->type=="Effect" &&
                        std::string(section)=="actions" && op=="ApplyEffect";
                    if(!unitRef && !effectRef)return;
                    const auto ref=node.find(unitRef?"catalog":"effect");
                    if(ref!=node.end() && ref->second.is<std::string>() &&
                       ref->second.get<std::string>()==id)
                        refs.push_back("trigger "+ruleName+"."+section+"."+op);
                });
            }
    }
    return refs;
}
bool RTSProject::removeCatalogObject(const std::string& id,std::string* error) {
    if(!catalog.find(id)) {
        if(error)*error="Unknown catalog ID: "+id;
        return false;
    }
    const auto refs=catalogReferences(id);
    if(!refs.empty()) {
        if(error)*error="Used by "+refs.front()+
            (refs.size()>1?" (+"+std::to_string(refs.size()-1)+" more)":"");
        return false;
    }
    try {
        RTSProject next=*this;
        if(!next.catalog.erase(id))return false;
        next.validate();
        *this=std::move(next);
        return true;
    } catch(const std::exception& e) {
        if(error)*error=e.what();
        return false;
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
