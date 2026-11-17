#pragma once
// Headless pre-package verification: all externally referenced Model meshes
// resolve under the map's directory and pass the exact runtime mesh reader.
#include "RTSProject.hpp"
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
    report.uniqueFiles=cache.size();
    return report;
}
