#pragma once
// Headless pre-package verification: all externally referenced Model meshes
// resolve under the map's directory and pass the exact runtime mesh reader.
#include "RTSProject.hpp"
#include "InstanceOverrides.hpp"
#include "RTSStaticMesh.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct RTSAssetAuditRow {
    std::string modelId;
    std::string reference;
    std::string error;
    std::uint32_t triangles=0;
    std::uint32_t vertices=0;
    bool valid()const{return error.empty();}
};
struct RTSAssetAudit {
    std::size_t models=0,importedModels=0,uniqueFiles=0;
    std::size_t triangleInstances=0,uniqueTriangles=0;
    std::vector<RTSAssetAuditRow> rows;
    bool valid()const {
        for(const auto& item:rows)if(!item.valid())return false;
        return true;
    }
};
inline RTSAssetAudit rtsAuditProjectAssets(
    const RTSProject& project,const std::filesystem::path& projectPath){
    RTSAssetAudit report;
    struct Cached {
        std::string error;
        std::uint32_t vertices=0,triangles=0;
        std::vector<RTSModelNode> nodes;
    };
    std::map<std::string,Cached> cache;
    for(const auto& entry:project.catalog.entries()){
        if(entry.type!="Model")continue;
        ++report.models;
        const auto fields=project.catalog.resolvedFields(entry.id);
        const auto it=fields.find("mesh");
        if(it==fields.end())continue; // legacy blank: procedural fallback
        RTSAssetAuditRow row;
        row.modelId=entry.id;
        if(!it->second.is<std::string>()){
            row.error="Model mesh property must be a string";
            report.rows.push_back(std::move(row));
            continue;
        }
        row.reference=it->second.get<std::string>();
        if(row.reference.empty() || row.reference=="procedural.unit")
            continue;
        ++report.importedModels;
        if(!rtsSafeMeshReference(row.reference)){
            row.error="Unsupported or unsafe static mesh reference";
            report.rows.push_back(std::move(row));
            continue;
        }
        auto found=cache.find(row.reference);
        if(found==cache.end()){
            Cached compiled;
            try{
                const auto mesh=rtsReadStaticMesh(
                    rtsResolveMeshReference(projectPath,row.reference));
                compiled.vertices=static_cast<std::uint32_t>(
                    mesh.vertices.size()/8);
                compiled.triangles=static_cast<std::uint32_t>(
                    mesh.indices.size()/3);
                compiled.nodes=mesh.nodes;
                report.uniqueTriangles+=compiled.triangles;
            }catch(const std::exception& e){
                compiled.error=e.what();
            }
            found=cache.emplace(row.reference,std::move(compiled)).first;
        }
        row.error=found->second.error;
        row.vertices=found->second.vertices;
        row.triangles=found->second.triangles;
        report.triangleInstances+=row.triangles;
        report.rows.push_back(std::move(row));
    }
    const auto verifyAttachments=[&](const ActorVisual& actor,const std::string& id){
        if(actor.attachments.empty())return;
        auto parent=cache.find(actor.mesh);
        std::vector<RTSModelNode> extra;
        const std::vector<RTSModelNode>* nodes=nullptr;
        if(parent!=cache.end() && parent->second.error.empty())nodes=&parent->second.nodes;
        else try{extra=rtsReadStaticMesh(rtsResolveMeshReference(projectPath,actor.mesh)).nodes;nodes=&extra;}
             catch(const std::exception&){}
        for(const auto& a:actor.attachments){
            if(!nodes || !rtsFindUniqueModelNode(*nodes,a.socket)){
                RTSAssetAuditRow row;
                row.modelId=id;row.reference=actor.mesh;
                row.error="Missing or ambiguous attachment socket: "+a.socket;
                report.rows.push_back(std::move(row));
            }
        }
    };
    const auto diagnose=[&](const std::string& id,const auto& resolve){
        try{verifyAttachments(resolve(),id);}
        catch(const std::exception& ex){
            RTSAssetAuditRow row;row.modelId=id;row.error=ex.what();
            report.rows.push_back(std::move(row));
        }
    };
    for(const auto& entry:project.catalog.entries())if(entry.type=="Actor"){
        const auto fields=project.catalog.resolvedFields(entry.id);
        const auto it=fields.find("attachments");
        if(it==fields.end() || (it->second.is<picojson::array>() &&
                               it->second.get<picojson::array>().empty()))continue;
        diagnose(entry.id,[&]{return actorFromCatalog(project.catalog,entry.id);});
    }
    for(const auto& placed:project.placements)if(!placed.overrides.empty()){
        if(placed.kind=="Doodad")
            diagnose(placed.id,[&]{return resolvePlacedDoodad(project,placed);});
        else if(placed.kind=="Unit")
            diagnose(placed.id,[&]{return resolvePlacedUnit(project,placed).actor;});
    }
    report.uniqueFiles=cache.size();
    return report;
}
