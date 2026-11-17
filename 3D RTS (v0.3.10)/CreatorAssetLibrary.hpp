#pragma once
// F7 presentation adapter; catalog changes occur in headless EditorSession.
#include "RTSAssetLibrary.hpp"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <vector>

namespace CreatorAssetLibrary {
struct Browser {
    std::string projectPath;
    std::vector<RTSAssetRecord> records;
    bool ready=false;
};
template<class App>
bool draw(App& app){
    static Browser browser;
    if(browser.projectPath!=app.projectPath){
        browser.projectPath=app.projectPath;
        browser.ready=false;
    }
    if(!ImGui::CollapsingHeader("Imported 3D asset library",
                               ImGuiTreeNodeFlags_DefaultOpen))return false;
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
        "Convert .glb/.gltf using tools/import_gltf.py first. "
        "Compiled assets remain usable in the packaged editor without Python.");
    if(browser.records.empty())
        ImGui::TextDisabled("No compiled models yet. Put them beside the map in assets/.");
    const float avail=ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##compiledMeshLibrary",
        ImVec2(0,std::min(215.f*ImGui::GetFontSize()/16.5f,
                          std::max(60.f,avail*.35f))),true);
    for(std::size_t i=0;i<browser.records.size();++i){
        const auto& asset=browser.records[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextWrapped("%s",asset.reference.c_str());
        if(!asset.valid()){
            ImGui::TextColored(ImVec4(1.f,.44f,.38f,1.f),
                "Invalid: %s",asset.error.c_str());
        }else{
            ImGui::TextDisabled("%u triangles, %u vertices, texture %u x %u",
                asset.triangles,asset.vertices,
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
    ImGui::TextDisabled("One undo step for Model + Actor; no files are rewritten.");
    return false;
}
} // namespace CreatorAssetLibrary
