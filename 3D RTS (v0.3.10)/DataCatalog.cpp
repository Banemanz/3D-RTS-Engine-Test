#include "DataCatalog.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <utility>

namespace {
bool validId(const std::string& s) {
    if(s.empty() || s.size()>128)return false;
    for(char c:s)if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') ||
                      (c>='0'&&c<='9') || c=='_' || c=='.' || c=='-')) return false;
    return true;
}
double numeric(const picojson::object& fields,const char* key,double fallback) {
    auto i=fields.find(key);
    if(i==fields.end())return fallback;
    if(!i->second.is<double>())throw std::invalid_argument(std::string(key)+" must be a number");
    double n=i->second.get<double>();
    if(!std::isfinite(n))throw std::invalid_argument(std::string(key)+" must be finite");
    return n;
}
std::string stringField(const picojson::object& fields,const char* key,const std::string& fallback) {
    auto i=fields.find(key);
    if(i==fields.end())return fallback;
    if(!i->second.is<std::string>())throw std::invalid_argument(std::string(key)+" must be a string");
    return i->second.get<std::string>();
}
Vec3 vec3Field(const picojson::object& fields,const char* key,const Vec3& fallback) {
    auto i=fields.find(key);
    if(i==fields.end())return fallback;
    if(!i->second.is<picojson::array>())throw std::invalid_argument(std::string(key)+" must be an array");
    const auto& a=i->second.get<picojson::array>();
    if(a.size()!=3 || !a[0].is<double>() || !a[1].is<double>() || !a[2].is<double>())
        throw std::invalid_argument(std::string(key)+" must contain exactly three numbers");
    Vec3 v(float(a[0].get<double>()),float(a[1].get<double>()),float(a[2].get<double>()));
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))
        throw std::invalid_argument(std::string(key)+" must be finite");
    return v;
}
picojson::value vector3(float x,float y,float z) {
    picojson::array a{picojson::value(double(x)),picojson::value(double(y)),picojson::value(double(z))};
    return picojson::value(a);
}
CatalogEntry make(const std::string& type,const std::string& id,const std::string& parent,
                  picojson::object fields) {
    CatalogEntry e;e.type=type;e.id=id;e.parent=parent;e.fields=std::move(fields);return e;
}
}
const CatalogEntry* DataCatalog::find(const std::string& id)const {
    const auto it=idIndex_.find(id);
    if(it==idIndex_.end() || it->second>=entries_.size())return nullptr;
    const CatalogEntry& entry=entries_[it->second];
    return entry.id==id?&entry:nullptr;
}
CatalogEntry* DataCatalog::find(const std::string& id) {
    const auto it=idIndex_.find(id);
    if(it==idIndex_.end() || it->second>=entries_.size())return nullptr;
    CatalogEntry& entry=entries_[it->second];
    return entry.id==id?&entry:nullptr;
}
bool DataCatalog::add(CatalogEntry e,std::string* error) {
    if(!validId(e.id) || !validId(e.type) || (!e.parent.empty() && !validId(e.parent)) ||
       idIndex_.count(e.id)!=0 || entries_.size()>=10000) {
        if(error)*error="Invalid or duplicate catalog ID/type/parent";
        return false;
    }
    entries_.push_back(std::move(e));
    idIndex_.emplace(entries_.back().id,entries_.size()-1);
    return true;
}
bool DataCatalog::cloneDerived(const std::string& original,const std::string& newId) {
    const CatalogEntry* e=find(original);
    if(!e || !validId(newId) || find(newId))return false;
    return add(make(e->type,newId,original,{}));
}
bool DataCatalog::rename(const std::string& oldId,const std::string& newId) {
    if(oldId==newId || !validId(newId) || !find(oldId) || find(newId))return false;
    validate();
    const std::size_t index=idIndex_.at(oldId);
    const std::string type=entries_[index].type;
    for(auto& entry:entries_) {
        if(entry.parent==oldId)entry.parent=newId;
        const char* ref=(type=="Actor" && entry.type=="Unit")?"actor":
                        (type=="Model" && entry.type=="Actor")?"model":nullptr;
        if(ref) {
            auto field=entry.fields.find(ref);
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Unit" && entry.type=="Unit") {
            auto field=entry.fields.find("trainable_units");
            if(field!=entry.fields.end() && field->second.is<picojson::array>())
                for(auto& candidate:field->second.get<picojson::array>())
                    if(candidate.is<std::string>() &&
                       candidate.get<std::string>()==oldId)
                        candidate=picojson::value(newId);
        }
    }
    entries_[index].id=newId;
    idIndex_.erase(oldId);
    idIndex_.emplace(newId,index);
    validate();
    return true;
}
bool DataCatalog::erase(const std::string& id) {
    const auto it=idIndex_.find(id);
    if(it==idIndex_.end())return false;
    validate(); // reject any unauthorized direct ID mutation
    const std::size_t at=it->second;
    entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(at));
    idIndex_.erase(id);
    for(std::size_t i=at;i<entries_.size();++i)idIndex_[entries_[i].id]=i;
    return true;
}
bool DataCatalog::setField(const std::string& id,const std::string& key,
                            const picojson::value& value) {
    CatalogEntry* e=find(id);
    if(!e || !validId(key))return false;
    e->fields[key]=value;return true;
}
void DataCatalog::resolveInto(const std::string& id,picojson::object& output,
                              std::vector<std::string>& stack)const {
    const auto* e=find(id);
    if(!e)throw std::invalid_argument("Missing catalog ID: "+id);
    if(std::find(stack.begin(),stack.end(),id)!=stack.end())
        throw std::invalid_argument("Catalog inheritance cycle at: "+id);
    stack.push_back(id);
    if(!e->parent.empty()) {
        const auto* p=find(e->parent);
        if(!p || p->type!=e->type)
            throw std::invalid_argument("Missing/mismatched parent of: "+id);
        resolveInto(e->parent,output,stack);
    }
    for(const auto& field:e->fields)output[field.first]=field.second;
    stack.pop_back();
}
picojson::object DataCatalog::resolvedFields(const std::string& id)const {
    picojson::object result;
    std::vector<std::string> stack;
    resolveInto(id,result,stack);
    return result;
}
void DataCatalog::validate()const {
    if(idIndex_.size()!=entries_.size())
        throw std::invalid_argument("Catalog ID index is inconsistent");
    for(std::size_t i=0;i<entries_.size();++i) {
        const auto it=idIndex_.find(entries_[i].id);
        if(it==idIndex_.end() || it->second!=i)
            throw std::invalid_argument("Catalog entry IDs were changed outside the catalog API");
    }
    std::set<std::string> ids;
    for(const auto& e:entries_) {
        if(!validId(e.id) || !validId(e.type) || (!e.parent.empty() && !validId(e.parent)))
            throw std::invalid_argument("Invalid catalog type or ID");
        if(!ids.insert(e.id).second)throw std::invalid_argument("Duplicate catalog ID: "+e.id);
        (void)resolvedFields(e.id);
    }
    // Only known engine-facing references are checked; arbitrary extension
    // fields are allowed and round-trip losslessly.
    for(const auto& e:entries_) {
        auto fields=resolvedFields(e.id);
        const char* reference = e.type=="Unit"?"actor":(e.type=="Actor"?"model":nullptr);
        const char* type = e.type=="Unit"?"Actor":"Model";
        if(reference && fields.count(reference)) {
            std::string id=stringField(fields,reference,"");
            const auto* linked=find(id);
            if(!id.empty() && (!linked || linked->type!=type))
                throw std::invalid_argument("Broken "+std::string(reference)+" reference in "+e.id);
        }
    }
}
picojson::value DataCatalog::serialize()const {
    picojson::array result;
    for(const auto& e:entries_) {
        picojson::object ob;
        ob["id"]=picojson::value(e.id);
        ob["type"]=picojson::value(e.type);
        if(!e.parent.empty())ob["parent"]=picojson::value(e.parent);
        ob["fields"]=picojson::value(e.fields);
        result.emplace_back(ob);
    }
    return picojson::value(result);
}
DataCatalog DataCatalog::deserialize(const picojson::value& value) {
    if(!value.is<picojson::array>())throw std::invalid_argument("catalog must be an array");
    DataCatalog result;
    for(const auto& v:value.get<picojson::array>()) {
        if(!v.is<picojson::object>())throw std::invalid_argument("catalog entry must be object");
        const auto& ob=v.get<picojson::object>();
        if(!ob.count("id")||!ob.count("type")||!ob.count("fields")||
           !ob.at("id").is<std::string>()||!ob.at("type").is<std::string>()||
           !ob.at("fields").is<picojson::object>())
            throw std::invalid_argument("catalog entry requires id,type,fields");
        CatalogEntry e;
        e.id=ob.at("id").get<std::string>();e.type=ob.at("type").get<std::string>();
        e.fields=ob.at("fields").get<picojson::object>();
        if(ob.count("parent")) {
            if(!ob.at("parent").is<std::string>())throw std::invalid_argument("parent must be string");
            e.parent=ob.at("parent").get<std::string>();
        }
        if(!result.add(std::move(e)))throw std::invalid_argument("Invalid/duplicate catalog entry");
    }
    result.validate();
    return result;
}
DataCatalog DataCatalog::defaults() {
    DataCatalog c;
    c.add(make("Model","procedural_soldier","",{
        {"mesh",picojson::value("procedural.unit")},
        {"scale",vector3(1,1,1)},{"material",picojson::value("default")}
    }));
    c.add(make("Model","procedural_building","",{
        {"mesh",picojson::value("procedural.unit")},{"scale",vector3(1,1,1)}
    }));
    c.add(make("Actor","ranger_actor","",{
        {"model",picojson::value("procedural_soldier")},
        {"tint",vector3(.22f,.58f,.96f)},{"opacity",picojson::value(1.0)}
    }));
    c.add(make("Actor","worker_actor","ranger_actor",{
        {"tint",vector3(.20f,.90f,.65f)}
    }));
    c.add(make("Actor","rift_raider_actor","",{
        {"model",picojson::value("procedural_soldier")},
        {"tint",vector3(.76f,.24f,.88f)}
    }));
    c.add(make("Actor","human_building_actor","",{
        {"model",picojson::value("procedural_building")},
        {"tint",vector3(.24f,.61f,.86f)}
    }));
    c.add(make("Actor","hybrid_building_actor","human_building_actor",{
        {"tint",vector3(.72f,.24f,.85f)}
    }));
    c.add(make("Unit","ranger","",{
        {"name",picojson::value("Frontier Ranger")},{"actor",picojson::value("ranger_actor")},
        {"faction",picojson::value("Humanity")},{"max_hp",picojson::value(45.0)},
        {"armor",picojson::value(0.0)},{"damage",picojson::value(7.0)},
        {"attack_range",picojson::value(6.5)},{"attack_cooldown",picojson::value(.75)},
        {"aggro_range",picojson::value(10.5)},{"sight_range",picojson::value(14.0)},
        {"move_speed",picojson::value(9.5)},
        {"supply_cost",picojson::value(1.0)},{"mineral_cost",picojson::value(50.0)},
        {"build_time",picojson::value(6.0)},{"visual_scale",picojson::value(.92)}
    }));
    c.add(make("Unit","worker","ranger",{
        {"name",picojson::value("Human Worker")},{"actor",picojson::value("worker_actor")},
        {"max_hp",picojson::value(40.0)},{"damage",picojson::value(2.0)},
        {"attack_range",picojson::value(1.5)},{"attack_cooldown",picojson::value(1.3)},
        {"move_speed",picojson::value(7.0)},{"visual_scale",picojson::value(.72)},
        {"mineral_cost",picojson::value(40.0)},{"sight_range",picojson::value(12.0)},
        {"is_worker",picojson::value(true)},{"build_time",picojson::value(4.5)}
    }));
    c.add(make("Unit","rift_raider","",{
        {"name",picojson::value("Rift Raider")},{"actor",picojson::value("rift_raider_actor")},
        {"faction",picojson::value("Hybrid")},{"max_hp",picojson::value(160.0)},
        {"armor",picojson::value(3.0)},{"damage",picojson::value(18.0)},
        {"attack_range",picojson::value(1.85)},{"attack_cooldown",picojson::value(1.05)},
        {"aggro_range",picojson::value(11.0)},{"sight_range",picojson::value(14.0)},
        {"move_speed",picojson::value(8.2)},
        {"supply_cost",picojson::value(3.0)},{"mineral_cost",picojson::value(150.0)},
        {"visual_scale",picojson::value(1.28)}
    }));
    c.add(make("Unit","command_center","ranger",{
        {"name",picojson::value("Command Center")},{"actor",picojson::value("human_building_actor")},
        {"max_hp",picojson::value(700.0)},{"armor",picojson::value(2.0)},
        {"damage",picojson::value(0.0)},{"attack_range",picojson::value(0.0)},
        {"aggro_range",picojson::value(0.0)},{"sight_range",picojson::value(18.0)},
        {"visual_scale",picojson::value(2.8)},
        {"supply_cost",picojson::value(0.0)},{"is_structure",picojson::value(true)},
        {"trainable_units",picojson::value(picojson::array{picojson::value("worker")})}
    }));
    c.add(make("Unit","rift_bastion","command_center",{
        {"name",picojson::value("Rift Bastion")},{"actor",picojson::value("hybrid_building_actor")},
        {"faction",picojson::value("Hybrid")}
    }));
    c.add(make("Unit","barracks","command_center",{
        {"name",picojson::value("Barracks")},{"max_hp",picojson::value(350.0)},
        {"visual_scale",picojson::value(2.0)},{"build_time",picojson::value(9.0)},
        {"mineral_cost",picojson::value(150.0)},
        {"trainable_units",picojson::value(picojson::array{picojson::value("ranger")})}
    }));
    c.validate();
    return c;
}
UnitData unitFromCatalog(const DataCatalog& catalog,const std::string& id) {
    const auto* item=catalog.find(id);
    if(!item || item->type!="Unit")throw std::invalid_argument("Not a Unit catalog entry: "+id);
    auto f=catalog.resolvedFields(id);
    UnitData d;
    d.resourceName=stringField(f,"name",id);
    const auto faction=stringField(f,"faction","Humanity");
    if(faction=="Humanity")d.faction=Faction::Humanity;
    else if(faction=="Hybrid")d.faction=Faction::Hybrid;
    else throw std::invalid_argument("Unsupported faction: "+faction);
    d.max_hp=float(numeric(f,"max_hp",d.max_hp));
    d.armor=float(numeric(f,"armor",d.armor));
    d.damage=float(numeric(f,"damage",d.damage));
    d.attack_range=float(numeric(f,"attack_range",d.attack_range));
    d.attack_cooldown=float(numeric(f,"attack_cooldown",d.attack_cooldown));
    d.aggro_range=float(numeric(f,"aggro_range",d.aggro_range));
    d.sight_range=float(numeric(f,"sight_range",d.sight_range));
    d.move_speed=float(numeric(f,"move_speed",d.move_speed));
    d.supply_cost=int(numeric(f,"supply_cost",d.supply_cost));
    d.mineral_cost=int(numeric(f,"mineral_cost",d.mineral_cost));
    d.build_time=float(numeric(f,"build_time",d.build_time));
    d.visual_scale=float(numeric(f,"visual_scale",d.visual_scale));
    if(d.max_hp<=0 || d.armor<0 || d.damage<0 || d.attack_range<0 ||
       d.attack_cooldown<=0 || d.aggro_range<0 ||
       d.sight_range<0 || d.sight_range>128 || d.move_speed<0 ||
       d.supply_cost<0 || d.mineral_cost<0 || d.build_time<0 || d.visual_scale<=0)
        throw std::invalid_argument("Invalid unit gameplay stats: "+id);
    std::string actor=stringField(f,"actor","");
    if(!actor.empty()) {
        ActorVisual visual=actorFromCatalog(catalog,actor);
        d.faction_color=visual.tint;
    }
    return d;
}
ActorVisual actorFromCatalog(const DataCatalog& catalog,const std::string& actorId) {
    const auto* entry=catalog.find(actorId);
    if(!entry || entry->type!="Actor")throw std::invalid_argument("Not an Actor: "+actorId);
    auto f=catalog.resolvedFields(actorId);
    ActorVisual v;
    v.tint=vec3Field(f,"tint",v.tint);
    v.scale=vec3Field(f,"scale",v.scale);
    v.opacity=float(numeric(f,"opacity",1.0));
    v.animation=stringField(f,"animation","");
    v.sound=stringField(f,"sound","");
    auto modelId=stringField(f,"model","");
    if(!modelId.empty()){
        auto m=catalog.resolvedFields(modelId);
        v.mesh=stringField(m,"mesh",v.mesh);
        v.texture=stringField(m,"texture","");
        Vec3 modelScale=vec3Field(m,"scale",{1,1,1});
        v.scale=Vec3(v.scale.x*modelScale.x,v.scale.y*modelScale.y,v.scale.z*modelScale.z);
    }
    if(v.opacity<0 || v.opacity>1 || v.scale.x<=0 || v.scale.y<=0 || v.scale.z<=0)
        throw std::invalid_argument("Invalid visual scale or opacity: "+actorId);
    return v;
}
