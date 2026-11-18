#include "InstanceOverrides.hpp"
#include <stdexcept>
#include <utility>

namespace {
const picojson::object* section(const ProjectPlacement& placement,const char* key) {
    auto i=placement.overrides.find(key);
    if(i==placement.overrides.end())return nullptr;
    if(!i->second.is<picojson::object>())
        throw std::invalid_argument("Placement "+placement.id+": '"+key+"' override must be an object");
    return &i->second.get<picojson::object>();
}
void apply(DataCatalog& catalog,const std::string& id,const picojson::object* fields) {
    if(!fields)return;
    for(const auto& [key,value]:*fields)
        if(!catalog.setField(id,key,value))
            throw std::invalid_argument("Invalid catalog instance property: "+id+"."+key);
}
bool flag(const DataCatalog& catalog,const std::string& id,const char* key) {
    auto fields=catalog.resolvedFields(id);
    auto i=fields.find(key);
    if(i==fields.end())return false;
    if(!i->second.is<bool>())
        throw std::invalid_argument(std::string("Catalog flag ")+id+"."+key+" must be bool");
    return i->second.get<bool>();
}
std::string linked(const DataCatalog& catalog,const std::string& id,const char* key) {
    const auto resolved=catalog.resolvedFields(id);
    auto i=resolved.find(key);
    if(i==resolved.end())return "";
    if(!i->second.is<std::string>())
        throw std::invalid_argument(std::string("Catalog reference ")+id+"."+key+" must be string");
    return i->second.get<std::string>();
}
}
ResolvedUnitInstance resolvePlacedUnit(const RTSProject& project,
                                       const ProjectPlacement& placement) {
    if(placement.kind!="Unit")
        throw std::invalid_argument("Instance resolver expects a Unit placement");
    const auto* u=section(placement,"unit");
    const auto* a=section(placement,"actor");
    const auto* m=section(placement,"model");
    if(!u && !a && !m) {
        ResolvedUnitInstance result;
        result.unit=unitFromCatalog(project.catalog,placement.catalogId);
        result.isStructure=flag(project.catalog,placement.catalogId,"is_structure");
        result.isWorker=flag(project.catalog,placement.catalogId,"is_worker");
        const auto actorId=linked(project.catalog,placement.catalogId,"actor");
        if(!actorId.empty()) {
            result.actor=actorFromCatalog(project.catalog,actorId);
            result.hasActor=true;
        }
        return result;
    }

    // This temporary copy is only created once per overridden instance at
    // authoring time/map initialization, never in the simulation tick.
    DataCatalog effective=project.catalog;
    apply(effective,placement.catalogId,u);
    const std::string actorId=linked(effective,placement.catalogId,"actor");
    if(a && actorId.empty())
        throw std::invalid_argument("Actor overrides require linked Actor");
    if(!actorId.empty()) {
        if(!effective.find(actorId) || effective.find(actorId)->type!="Actor")
            throw std::invalid_argument("Invalid Actor ID in placement");
        apply(effective,actorId,a);
        const auto modelId=linked(effective,actorId,"model");
        if(m && modelId.empty())
            throw std::invalid_argument("Model overrides require linked Model");
        if(!modelId.empty())apply(effective,modelId,m);
    } else if(m)
        throw std::invalid_argument("Model overrides require linked Actor");
    ResolvedUnitInstance result;
    result.unit=unitFromCatalog(effective,placement.catalogId);
    result.isStructure=flag(effective,placement.catalogId,"is_structure");
    result.isWorker=flag(effective,placement.catalogId,"is_worker");
    if(!actorId.empty()){
        result.actor=actorFromCatalog(effective,actorId);
        result.hasActor=true;
    }
    return result;
}

ActorVisual resolvePlacedDoodad(const RTSProject& project,
                                const ProjectPlacement& placement) {
    if(placement.kind!="Doodad")
        throw std::invalid_argument("Doodad resolver expects Doodad placement");
    const auto* entry=project.catalog.find(placement.catalogId);
    if(!entry || (entry->type!="Actor" && entry->type!="Model"))
        throw std::invalid_argument("Doodad has missing Actor or Model reference");
    const auto* actorOverride=section(placement,"actor");
    const auto* modelOverride=section(placement,"model");
    if(entry->type=="Actor" && !actorOverride && !modelOverride)
        return actorFromCatalog(project.catalog,placement.catalogId);
    DataCatalog catalog=project.catalog;
    std::string actorId=placement.catalogId;
    if(entry->type=="Model") {
        if(actorOverride)
            throw std::invalid_argument("Model-based doodad has no Actor to override");
        const std::string modelId=placement.catalogId;
        apply(catalog,modelId,modelOverride);
        std::string temp="instance_model_actor";
        while(catalog.find(temp))temp+="_";
        CatalogEntry actor;
        actor.id=temp;actor.type="Actor";
        actor.fields["model"]=picojson::value(modelId);
        if(!catalog.add(std::move(actor)))
            throw std::invalid_argument("Cannot create model preview Actor");
        actorId=temp;
    }else{
        apply(catalog,actorId,actorOverride);
        const std::string modelId=linked(catalog,actorId,"model");
        if(modelOverride && modelId.empty())
            throw std::invalid_argument("Doodad requires linked Model");
        if(!modelId.empty())apply(catalog,modelId,modelOverride);
    }
    return actorFromCatalog(catalog,actorId);
}
