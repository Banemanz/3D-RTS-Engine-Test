#include "RTSUpgrades.hpp"
#include "RTSRequirements.hpp"
#include <cmath>
#include <set>
#include <stdexcept>

namespace {
double number(const picojson::object& fields,const char* key,double fallback){
    const auto it=fields.find(key);
    if(it==fields.end())return fallback;
    if(!it->second.is<double>())
        throw std::invalid_argument(std::string("Upgrade ")+key+" must be number");
    const double n=it->second.get<double>();
    if(!std::isfinite(n))
        throw std::invalid_argument(std::string("Upgrade ")+key+" must be finite");
    return n;
}
std::string idField(const picojson::object& fields,const char* key){
    const auto it=fields.find(key);
    if(it==fields.end())return {};
    if(!it->second.is<std::string>())
        throw std::invalid_argument(std::string("Upgrade ")+key+" must be an ID");
    return it->second.get<std::string>();
}
int integer(const picojson::object& fields,const char* key,int def,
            int low,int high){
    const double n=number(fields,key,def);
    if(n<low || n>high || std::floor(n)!=n)
        throw std::invalid_argument(std::string("Invalid Upgrade ")+key);
    return static_cast<int>(n);
}
float bonus(const picojson::object& fields,const char* key){
    const double n=number(fields,key,0.);
    if(n<0. || n>100.)
        throw std::invalid_argument(std::string("Invalid Upgrade ")+key);
    return static_cast<float>(n);
}
}
RTSUpgradeDefinition compileUpgrade(const DataCatalog& catalog,
                                    const std::string& id){
    const auto* entry=catalog.find(id);
    if(!entry || entry->type!="Upgrade")
        throw std::invalid_argument("Expected Upgrade reference: "+id);
    const auto fields=catalog.resolvedFields(id);
    RTSUpgradeDefinition d;
    d.id=id;
    d.maxLevel=integer(fields,"max_level",1,1,5);
    d.mineralCost=integer(fields,"mineral_cost",100,0,100000);
    d.researchTicks=static_cast<std::uint32_t>(
        integer(fields,"research_ticks",180,1,36000));
    d.damagePerLevel=bonus(fields,"damage_per_level");
    d.armorPerLevel=bonus(fields,"armor_per_level");
    d.unitId=idField(fields,"unit");
    if(!d.unitId.empty()){
        const auto* unit=catalog.find(d.unitId);
        if(!unit || unit->type!="Unit")
            throw std::invalid_argument("Upgrade unit target must be Unit: "+id);
    }
    d.requirementId=idField(fields,"requirement");
    if(!d.requirementId.empty()){
        const auto graph=compileRequirement(catalog,d.requirementId);
        if(rtsRequirementUsesTarget(graph))
            throw std::invalid_argument(
                "Research Requirement cannot inspect Target: "+id);
    }
    return d;
}
std::vector<std::string> unitResearchIds(const DataCatalog& catalog,
                                         const std::string& unitId){
    const auto* unit=catalog.find(unitId);
    if(!unit || unit->type!="Unit")
        throw std::invalid_argument("Research producer is not a Unit: "+unitId);
    const auto fields=catalog.resolvedFields(unitId);
    const auto it=fields.find("researchable_upgrades");
    if(it==fields.end())return {};
    if(!it->second.is<picojson::array>())
        throw std::invalid_argument("researchable_upgrades must be an array: "+unitId);
    const auto& values=it->second.get<picojson::array>();
    if(values.size()>12)
        throw std::invalid_argument("Too many researchable_upgrades: "+unitId);
    std::vector<std::string> result;
    std::set<std::string> seen;
    for(const auto& item:values){
        if(!item.is<std::string>())
            throw std::invalid_argument("researchable_upgrades needs IDs: "+unitId);
        const std::string& id=item.get<std::string>();
        (void)compileUpgrade(catalog,id);
        if(!seen.insert(id).second)
            throw std::invalid_argument("Duplicate research Upgrade: "+id);
        result.push_back(id);
    }
    return result;
}
