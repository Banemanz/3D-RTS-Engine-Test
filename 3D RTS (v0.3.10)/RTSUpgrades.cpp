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
float bonus(const picojson::object& fields,const char* key,double fallback=0.){
    const double n=number(fields,key,fallback);
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
    d.levels.assign(static_cast<std::size_t>(d.maxLevel),
        RTSUpgradeLevel{d.mineralCost,d.researchTicks,
                        d.damagePerLevel,d.armorPerLevel});
    // An empty table explicitly restores uniform research, even on
    // an inherited Upgrade with custom levels.
    const auto table=fields.find("levels");
    if(table!=fields.end()){
        if(!table->second.is<picojson::array>())
            throw std::invalid_argument("Upgrade levels must be an array: "+id);
        const auto& rows=table->second.get<picojson::array>();
        if(!rows.empty()){
            if(rows.size()!=d.levels.size())
                throw std::invalid_argument(
                    "Upgrade levels must contain exactly max_level rows: "+id);
            d.customLevels=true;
            for(std::size_t i=0;i<rows.size();++i){
                if(!rows[i].is<picojson::object>())
                    throw std::invalid_argument("Upgrade level must be an object: "+id);
                const auto& level=rows[i].get<picojson::object>();
                auto& step=d.levels[i];
                step.mineralCost=integer(level,"mineral_cost",d.mineralCost,0,100000);
                step.researchTicks=static_cast<std::uint32_t>(
                    integer(level,"research_ticks",
                            static_cast<int>(d.researchTicks),1,36000));
                step.damageBonus=bonus(level,"damage_bonus",d.damagePerLevel);
                step.armorBonus=bonus(level,"armor_bonus",d.armorPerLevel);
            }
        }
    }
    d.unitId=idField(fields,"unit");
    if(!d.unitId.empty()){
        const auto* unit=catalog.find(d.unitId);
        if(!unit || unit->type!="Unit")
            throw std::invalid_argument("Upgrade unit target must be Unit: "+id);
    }
    d.requirementId=idField(fields,"requirement");
    d.showRequirementId=idField(fields,"show_requirement");
    for(const auto& link:{std::make_pair(d.requirementId,"Use"),
                          std::make_pair(d.showRequirementId,"Show")}){
        if(link.first.empty())continue;
        const auto graph=compileRequirement(catalog,link.first);
        if(rtsRequirementUsesTarget(graph))
            throw std::invalid_argument(
                "Research "+std::string(link.second)+
                " Requirement cannot inspect Target: "+id);
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
