#pragma once
// F7 presentation adapter; catalog changes occur in headless EditorSession.
#include "RTSAssetLibrary.hpp"
#include "RTSGltfImport.hpp"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace CreatorAssetLibrary {
inline bool requestImportPopup=false;
struct Browser {
    std::string projectPath;
    std::vector<RTSAssetRecord> records;
    bool ready=false;
    char source[2048]{};
    char output[128]="imported_model";
    std::string importNotice;
    std::filesystem::path folder;
    std::vector<std::filesystem::directory_entry> files;
    bool folderDirty=true;
};
inline void drawImport(Browser& browser){
    if(ImGui::Button("Import glTF / GLB...") || requestImportPopup){
        requestImportPopup=false;
        if(browser.folder.empty())browser.folder=std::filesystem::absolute(browser.projectPath).parent_path();
        browser.folderDirty=true;
        ImGui::OpenPopup("Import static model");
    }
    ImGui::SetNextWindowSize(ImVec2(620,540),ImGuiCond_FirstUseEver);
    if(!ImGui::BeginPopupModal("Import static model",nullptr,ImGuiWindowFlags_NoSavedSettings))return;
    ImGui::TextWrapped("Choose a static mesh with normals and one material. PNG/JPEG base color is supported. Animation, hierarchies and PBR maps are not yet supported.");
    ImGui::TextWrapped("Folder: %s",browser.folder.u8string().c_str());
    if(ImGui::Button("Parent folder")){
        browser.folder=browser.folder.parent_path();browser.folderDirty=true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Project folder")){
        browser.folder=std::filesystem::absolute(browser.projectPath).parent_path();browser.folderDirty=true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Refresh"))browser.folderDirty=true;
    if(browser.folderDirty){
        browser.files.clear();browser.folderDirty=false;
        std::error_code ec;
        std::filesystem::directory_iterator it(browser.folder,ec),end;
        std::size_t visited=0;
        while(!ec && it!=end && visited++<2048){
            auto ext=it->path().extension().string();
            std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(it->is_directory(ec) || ext==".glb" || ext==".gltf")browser.files.push_back(*it);
            it.increment(ec);
        }
        std::sort(browser.files.begin(),browser.files.end(),[](const auto& a,const auto& b){return a.path()<b.path();});
        if(ec)browser.importNotice="Cannot browse folder: "+ec.message();
    }
    ImGui::BeginChild("##sourceFiles",ImVec2(0,140),true);
    for(const auto& file:browser.files){
        std::error_code ec;const bool directory=file.is_directory(ec);
        const auto label=(directory?"[Folder] ":"")+file.path().filename().u8string();
        if(ImGui::Selectable(label.c_str())){
            if(directory){browser.folder=file.path();browser.folderDirty=true;}
            else{
                const auto path=file.path().u8string();
                if(path.size()<sizeof(browser.source))std::snprintf(browser.source,sizeof(browser.source),"%s",path.c_str());
                auto stem=file.path().stem().u8string();
                for(char& c:stem)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))c='_';
                std::snprintf(browser.output,sizeof(browser.output),"%s",stem.c_str());
            }
        }
    }
    ImGui::EndChild();
    ImGui::TextUnformatted("Source file (full path, or relative to this map)");
    ImGui::SetNextItemWidth(-1);ImGui::InputText("##sourcePath",browser.source,sizeof(browser.source));
    ImGui::TextUnformatted("Output name (without extension)");
    ImGui::SetNextItemWidth(-1);ImGui::InputText("##outputName",browser.output,sizeof(browser.output));
    ImGui::TextWrapped("Creates assets/%s.rtsmesh. Existing files are never overwritten. Register Model + Actor after import; Ctrl+S saves catalog changes.",browser.output);
    if(ImGui::Button("Import model")){
        try{
            auto source=std::filesystem::u8path(browser.source);
            if(source.is_relative())source=std::filesystem::absolute(browser.projectPath).parent_path()/source;
            const std::string name=browser.output;
            if(name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
                throw std::invalid_argument("Use letters, numbers, underscores or hyphens for the output name");
            rtsImportGltf(source,browser.projectPath,"assets/"+name+".rtsmesh");
            browser.ready=false;
            browser.importNotice="Imported assets/"+name+".rtsmesh. Register it below to create Model + Actor.";
            ImGui::CloseCurrentPopup();
        }catch(const std::exception& e){browser.importNotice=e.what();}
    }
    ImGui::SameLine();if(ImGui::Button("Close"))ImGui::CloseCurrentPopup();
    if(!browser.importNotice.empty())ImGui::TextWrapped("%s",browser.importNotice.c_str());
    ImGui::EndPopup();
};
template<class App>
bool draw(App& app){
    static Browser browser;
    if(browser.projectPath!=app.projectPath){
        browser.projectPath=app.projectPath;
        browser.ready=false;
        browser.folder.clear();browser.importNotice.clear();
    }
    if(!ImGui::CollapsingHeader("Imported 3D asset library",
                               ImGuiTreeNodeFlags_DefaultOpen))return false;
    drawImport(browser);
    if(!browser.importNotice.empty())ImGui::TextWrapped("%s",browser.importNotice.c_str());
    // Do not rescan 3D files on each GUI frame.
    const bool refresh=ImGui::SmallButton("Scan / refresh assets");
    if(refresh || !browser.ready){
        browser.records=rtsDiscoverStaticAssets(app.projectPath);
        browser.ready=true;
        ++app.assetRevision; // next world frame explicitly reloads GPU cache
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu compiled models in assets/",browser.records.size());
    ImGui::TextWrapped(
        "Import a model, then register it to create linked Model + Actor entries. "
        "Native import and compiled assets require no Python installation.");
    if(browser.records.empty())
        ImGui::TextDisabled("No compiled models yet. Put them beside the map in assets/.");
    const float avail=ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##compiledMeshLibrary",
        ImVec2(0,std::min(215.f*ImGui::GetFontSize()/16.5f,
                          std::max(120.f*ImGui::GetFontSize()/16.5f,avail*.35f))),true);
    for(std::size_t i=0;i<browser.records.size();++i){
        const auto& asset=browser.records[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextWrapped("%s",asset.reference.c_str());
        if(!asset.valid()){
            ImGui::TextColored(ImVec4(1.f,.44f,.38f,1.f),
                "Invalid: %s",asset.error.c_str());
        }else{
            ImGui::TextDisabled("%u tris, %u verts, %u materials, first texture %u x %u",
                asset.triangles,asset.vertices,asset.surfaces,
                asset.textureWidth,asset.textureHeight);
            if(ImGui::SmallButton("Register Model + Actor")){
                if(app.editor.importStaticMesh(app.project,app.projectPath,
                                               asset.reference)){
                    ImGui::PopID();
                    ImGui::EndChild();
                    return true; // caller reloads world and returns immediately
                }
            }
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Catalog registration is one undo step. Imported files remain on disk.");
    return false;
}
} // namespace CreatorAssetLibrary
