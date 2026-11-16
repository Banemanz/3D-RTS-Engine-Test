#pragma once
#include "RTSProject.hpp"
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Project-authored enemy research is opt-in. No new fields in existing
// projects -> no AI research input, no replay format/legacy digest changes.
struct RTSAIResearchSettings {
    bool enabled=false;
    int startingMinerals=100;
    int intervalTicks=30;
    std::vector<std::string> upgradePriorities;
};
inline RTSAIResearchSettings rtsAIResearchSettings(const RTSProject& project){
    RTSAIResearchSettings out;
    const auto& o=project.metadata;
    const auto flag=o.find("enemy_ai_research_enabled");
    if(flag!=o.end()){
        if(!flag->second.is<bool>())
            throw std::invalid_argument("enemy_ai_research_enabled must be boolean");
        out.enabled=flag->second.get<bool>();
    }
    const auto integer=[&](const char* key,int lo,int hi,int fallback){
        const auto found=o.find(key);
        if(found==o.end())return fallback;
        if(!found->second.is<double>())
            throw std::invalid_argument(std::string(key)+" must be an integer");
        const double value=found->second.get<double>();
        if(!std::isfinite(value) || std::floor(value)!=value ||
           value<double(lo) || value>double(hi))
            throw std::invalid_argument(std::string(key)+" is outside allowed range");
        return static_cast<int>(value);
    };
    out.startingMinerals=integer("enemy_ai_research_starting_minerals",
                                0,1000000,100);
    out.intervalTicks=integer("enemy_ai_research_interval_ticks",
                             2,3600,30);
    const auto list=o.find("enemy_ai_upgrade_priorities");
    if(list==o.end())return out;
    if(!list->second.is<picojson::array>() ||
       list->second.get<picojson::array>().size()>12)
        throw std::invalid_argument("enemy_ai_upgrade_priorities must have 0..12 IDs");
    std::set<std::string> unique;
    for(const auto& value:list->second.get<picojson::array>()){
        if(!value.is<std::string>())
            throw std::invalid_argument("AI Upgrade priority must be a catalog ID");
        const auto& id=value.get<std::string>();
        const auto* item=project.catalog.find(id);
        if(id.empty() || !item || item->type!="Upgrade" ||
           !unique.insert(id).second)
            throw std::invalid_argument("Missing or duplicate AI Upgrade priority: "+id);
        out.upgradePriorities.push_back(id);
    }
    return out;
}
