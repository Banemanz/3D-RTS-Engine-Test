#pragma once
#include "RTSProject.hpp"
#include <algorithm>
#include <string>
#include <vector>

// F7 data relationship graph: pure, headless and deterministically ordered.
// This is a model of typed references, never a speculative parser of arbitrary
// mod fields, so unrelated extension JSON is preserved and not mistaken for
// a catalog link. Edges resolved from parent fields retain provenance.
struct RTSCatalogLink {
    std::string from,to,field;
    bool inherited=false;
};
inline std::vector<RTSCatalogLink> rtsCatalogLinks(
        const RTSProject& project,const std::string& focus=""){
    std::vector<RTSCatalogLink> edges;
    for(const auto& entry:project.catalog.entries()){
        if(!entry.parent.empty() && (focus.empty() ||
              entry.id==focus || entry.parent==focus))
            edges.push_back({entry.id,entry.parent,"parent",false});
        const bool relevant=entry.type=="Unit" || entry.type=="Weapon" ||
            entry.type=="Ability" || entry.type=="Effect" ||
            entry.type=="Actor" || entry.type=="Validator" ||
            entry.type=="Requirement" || entry.type=="Upgrade";
        if(!relevant)continue;
        const auto fields=project.catalog.resolvedFields(entry.id);
        const auto single=[&](const char* field,const char* expected){
            const auto it=fields.find(field);
            if(it==fields.end() || !it->second.is<std::string>())return;
            const auto& id=it->second.get<std::string>();
            const auto* to=project.catalog.find(id);
            if(!to || to->type!=expected ||
               (!focus.empty() && entry.id!=focus && id!=focus))return;
            edges.push_back({entry.id,id,field,
                             entry.fields.count(field)==0});
        };
        const auto many=[&](const char* field,const char* expected){
            const auto it=fields.find(field);
            if(it==fields.end() || !it->second.is<picojson::array>())return;
            for(const auto& value:it->second.get<picojson::array>()){
                if(!value.is<std::string>())continue;
                const auto& id=value.get<std::string>();
                const auto* to=project.catalog.find(id);
                if(!to || to->type!=expected ||
                   (!focus.empty() && entry.id!=focus && id!=focus))continue;
                edges.push_back({entry.id,id,field,
                                 entry.fields.count(field)==0});
            }
        };
        if(entry.type=="Unit"){
            single("actor","Actor");
            single("weapon","Weapon");
            many("abilities","Ability");
            many("trainable_units","Unit");
            many("researchable_upgrades","Upgrade");
            single("requirement","Requirement");
            single("show_requirement","Requirement");
        }else if(entry.type=="Weapon")
            single("projectile","Projectile");
        else if(entry.type=="Ability"){
            single("effect","Effect");
            single("projectile","Projectile");
            single("requirement","Requirement");
            single("show_requirement","Requirement");
        }else if(entry.type=="Upgrade"){
            single("unit","Unit");
            single("requirement","Requirement");
            single("show_requirement","Requirement");
        }else if(entry.type=="Requirement"){
            single("validator","Validator");
            many("children","Requirement");
        }else if(entry.type=="Validator"){
            single("behavior","Behavior");
            single("unit","Unit");
            single("upgrade","Upgrade");
        }else if(entry.type=="Effect"){
            single("behavior","Behavior");
            many("children","Effect");
        }else if(entry.type=="Actor")single("model","Model");
    }
    std::sort(edges.begin(),edges.end(),[](const RTSCatalogLink& a,
                                           const RTSCatalogLink& b){
        if(a.from!=b.from)return a.from<b.from;
        if(a.field!=b.field)return a.field<b.field;
        if(a.to!=b.to)return a.to<b.to;
        return a.inherited<b.inherited;
    });
    return edges;
}
