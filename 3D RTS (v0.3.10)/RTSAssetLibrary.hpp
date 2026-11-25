#pragma once
// CPU-only project-local mesh discovery; F7 scans on explicit request only.
#include "RTSStaticMesh.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

struct RTSAssetRecord {
    std::string reference;
    std::string error;
    std::uint32_t vertices=0;
    std::uint32_t triangles=0;
    std::uint32_t textureWidth=0,textureHeight=0;
    std::uint32_t surfaces=0;
    std::array<float,3> minimum{{0,0,0}},maximum{{0,0,0}};
    std::uint64_t imageBytes=0;
    std::uint32_t texturedSurfaces=0;
    bool valid()const{return error.empty();}
};
// Locale-independent ASCII filtering matches the portable asset path rules.
inline bool rtsAssetMatches(const RTSAssetRecord& asset,const std::string& query){
    const auto fold=[](std::string s){
        for(char& c:s)if(c>='A' && c<='Z')c=char(c-'A'+'a');
        return s;
    };
    return fold(asset.reference).find(fold(query))!=std::string::npos;
}
inline std::vector<RTSAssetRecord> rtsDiscoverStaticAssets(
    const std::filesystem::path& projectFile,std::size_t limit=192){
    namespace fs=std::filesystem;
    std::vector<RTSAssetRecord> out;
    if(!limit)return out;
    limit=std::min<std::size_t>(limit,512);
    std::error_code ec;
    const auto root=fs::weakly_canonical(
        fs::absolute(projectFile).parent_path(),ec);
    if(ec)return out;
    const auto folder=root/"assets";
    if(!fs::is_directory(folder,ec))return out;
    fs::recursive_directory_iterator it(folder,
        fs::directory_options::skip_permission_denied,ec),end;
    std::size_t visited=0;
    while(!ec && it!=end && visited++<4096){
        const auto entry=*it;
        if(it.depth()>3)it.disable_recursion_pending();
        if(entry.is_symlink(ec)){
            if(!ec)it.disable_recursion_pending();
        }else if(!ec && entry.is_regular_file(ec) && !ec &&
                 entry.path().extension()==".rtsmesh"){
            const auto rel=fs::relative(entry.path(),root,ec);
            if(!ec){
                RTSAssetRecord item;
                item.reference=rel.generic_string();
                if(rtsSafeMeshReference(item.reference)){
                    try{
                        const auto mesh=rtsReadStaticMesh(
                            rtsResolveMeshReference(projectFile,item.reference));
                        item.vertices=static_cast<std::uint32_t>(mesh.vertices.size()/8);
                        item.triangles=static_cast<std::uint32_t>(mesh.indices.size()/3);
                        item.surfaces=mesh.surfaces.empty()?1u:
                            static_cast<std::uint32_t>(mesh.surfaces.size());
                        item.textureWidth=mesh.surfaces.empty()?mesh.width:
                            mesh.surfaces.front().width;
                        item.textureHeight=mesh.surfaces.empty()?mesh.height:
                            mesh.surfaces.front().height;
                        for(int a=0;a<3;++a)
                            item.minimum[a]=item.maximum[a]=mesh.vertices[a];
                        for(std::size_t v=0;v<mesh.vertices.size();v+=8)
                            for(int a=0;a<3;++a){
                                item.minimum[a]=std::min(item.minimum[a],mesh.vertices[v+a]);
                                item.maximum[a]=std::max(item.maximum[a],mesh.vertices[v+a]);
                            }
                        item.imageBytes=mesh.albedo.size();
                        item.texturedSurfaces=mesh.albedo.empty()?0u:1u;
                        for(const auto& part:mesh.surfaces){
                            item.imageBytes+=part.albedo.size();
                            if(!part.albedo.empty())++item.texturedSurfaces;
                        }
                    }catch(const std::exception& ex){item.error=ex.what();}
                    out.push_back(std::move(item));
                    if(out.size()>=limit)break;
                }
            }
        }
        if(ec)break;
        it.increment(ec);
    }
    std::sort(out.begin(),out.end(),[](const RTSAssetRecord& a,
                                       const RTSAssetRecord& b){
        return a.reference<b.reference;
    });
    return out;
}
