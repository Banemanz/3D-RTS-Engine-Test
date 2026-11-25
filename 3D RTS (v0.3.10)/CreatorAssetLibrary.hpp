#pragma once
// F7 presentation adapter; catalog changes occur in headless EditorSession.
#include "RTSAssetLibrary.hpp"
#include "RTSGltfImport.hpp"
#include "CreatorModelPreview.hpp"
#include "DataCatalog.hpp"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace CreatorAssetLibrary {
inline bool requestImportPopup=false;
inline bool requestNodeInspector=false;
struct Browser {
    std::string projectPath;
    std::vector<RTSAssetRecord> records;
    bool ready=false;
    char source[2048]{};
    char output[128]="imported_model";
    char filter[128]{};
    bool invalidOnly=false;
    bool replaceExisting=false;
    std::string nodeAsset;
    std::size_t selectedNode=0;
    std::vector<std::pair<std::string,ActorVisual>> previewActors;
    std::size_t previewActor=0; // 0 = bind pose, otherwise index + 1
    float previewTime=0;
    bool previewPlaying=false;
    std::string importNotice;
    std::filesystem::path folder;
    std::vector<std::filesystem::directory_entry> files;
    bool folderDirty=true;
};
inline void drawImport(Browser& browser){
    if(ImGui::Button("Import glTF / GLB...") || requestImportPopup){
        requestImportPopup=false;
        browser.replaceExisting=false;
        if(browser.folder.empty())browser.folder=std::filesystem::absolute(browser.projectPath).parent_path();
        browser.folderDirty=true;
        ImGui::OpenPopup("Import static model");
    }
    ImGui::SetNextWindowSize(ImVec2(620,540),ImGuiCond_FirstUseEver);
    if(!ImGui::BeginPopupModal("Import static model",nullptr,ImGuiWindowFlags_NoSavedSettings))return;
    ImGui::TextWrapped("Choose a static glTF scene with normals, up to 128 nodes and 32 material surfaces. PNG/JPEG base color is supported. Node TRS is baked; animation and PBR maps are not yet supported.");
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
    ImGui::Checkbox("Explicitly replace an existing compiled asset",&browser.replaceExisting);
    ImGui::TextWrapped("Target: assets/%s.rtsmesh. Replacement preserves unique node names; asset files are not part of map undo.",browser.output);
    if(ImGui::Button(browser.replaceExisting?"Replace compiled asset":"Import model")){
        try{
            auto source=std::filesystem::u8path(browser.source);
            if(source.is_relative())source=std::filesystem::absolute(browser.projectPath).parent_path()/source;
            const std::string name=browser.output;
            if(name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
                throw std::invalid_argument("Use letters, numbers, underscores or hyphens for the output name");
            if(browser.replaceExisting)
                rtsReimportGltf(source,browser.projectPath,"assets/"+name+".rtsmesh");
            else rtsImportGltf(source,browser.projectPath,"assets/"+name+".rtsmesh");
            browser.ready=false;
            browser.importNotice=browser.replaceExisting?
                "Replaced assets/"+name+".rtsmesh; existing Model and Actor links are retained.":
                "Imported assets/"+name+".rtsmesh. Register it below to create Model + Actor.";
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
        browser.filter[0]='\0';browser.invalidOnly=false;
        browser.nodeAsset.clear();browser.selectedNode=0;
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
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##assetFilter","Search asset paths...",browser.filter,sizeof(browser.filter));
    ImGui::Checkbox("Invalid assets only",&browser.invalidOnly);
    const float avail=ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##compiledMeshLibrary",
        ImVec2(0,std::min(215.f*ImGui::GetFontSize()/16.5f,
                          std::max(170.f*ImGui::GetFontSize()/16.5f,avail*.45f))),true);
    std::size_t matched=0;
    bool openNodes=false;
    for(std::size_t i=0;i<browser.records.size();++i){
        const auto& asset=browser.records[i];
        if(!rtsAssetMatches(asset,browser.filter) ||
           (browser.invalidOnly && asset.valid()))continue;
        ++matched;
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextWrapped("%s",asset.reference.c_str());
        if(!asset.valid()){
            ImGui::TextColored(ImVec4(1.f,.44f,.38f,1.f),
                "Invalid: %s",asset.error.c_str());
        }else{
            ImGui::TextDisabled("%u tris, %u verts, %u materials, first texture %u x %u",
                asset.triangles,asset.vertices,asset.surfaces,
                asset.textureWidth,asset.textureHeight);
            if(ImGui::TreeNode("Geometry details")){
                ImGui::Text("Size: %.2f x %.2f x %.2f",
                    asset.maximum[0]-asset.minimum[0],
                    asset.maximum[1]-asset.minimum[1],
                    asset.maximum[2]-asset.minimum[2]);
                ImGui::Text("Min: %.2f, %.2f, %.2f",
                    asset.minimum[0],asset.minimum[1],asset.minimum[2]);
                ImGui::Text("Max: %.2f, %.2f, %.2f",
                    asset.maximum[0],asset.maximum[1],asset.maximum[2]);
                ImGui::Text("%u textured surfaces / %.2f MiB RGBA",
                    asset.texturedSurfaces,double(asset.imageBytes)/(1024.0*1024.0));
                ImGui::TreePop();
            }
            if(ImGui::SmallButton("Model viewer")){
                browser.nodeAsset=asset.reference;
                browser.selectedNode=0;
                openNodes=true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%zu retained nodes",asset.nodes.size());
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
    if(!matched)ImGui::TextDisabled("No matching assets.");
    ImGui::EndChild();
    ImGui::TextDisabled("Catalog registration is one undo step. Imported files remain on disk.");
    const bool smokeInspector=requestNodeInspector;
    if(requestNodeInspector){
        requestNodeInspector=false;
        for(const auto& asset:browser.records)if(asset.valid() && !asset.nodes.empty()){
            browser.nodeAsset=asset.reference;
            browser.selectedNode=asset.nodes.size()-1;
            openNodes=true;break;
        }
        if(!openNodes)throw std::runtime_error("Smoke model node inspector: no retained nodes");
    }
    if(openNodes){
        browser.previewActors.clear();browser.previewActor=0;
        browser.previewTime=0;browser.previewPlaying=false;
        // Build this list once on opening rather than scan the whole catalog
        // and resolve inheritance on every preview frame.
        for(const auto& entry:app.project.catalog.entries())if(entry.type=="Actor"){
            const auto visual=actorFromCatalog(app.project.catalog,entry.id);
            if(visual.mesh==browser.nodeAsset && browser.previewActors.size()<128){
                browser.previewActors.emplace_back(entry.id,visual);
                if(smokeInspector && browser.previewActor==0 && !visual.rigidClip.tracks.empty()){
                    browser.previewActor=browser.previewActors.size();
                    browser.previewTime=visual.rigidClip.duration*.375f;
                }
            }
        }
        ImGui::OpenPopup("Model viewer");
    }
    ImGui::SetNextWindowSize(ImVec2(1040,620),ImGuiCond_FirstUseEver);
    if(ImGui::BeginPopupModal("Model viewer",nullptr,ImGuiWindowFlags_NoSavedSettings)){
        const auto found=std::find_if(browser.records.begin(),browser.records.end(),
            [&](const RTSAssetRecord& a){return a.reference==browser.nodeAsset;});
        if(found==browser.records.end() || !found->valid())
            ImGui::TextWrapped("This asset is unavailable.");
        else{
            const auto& asset=*found;
            ImGui::TextWrapped("%s",asset.reference.c_str());
            ImGui::TextDisabled("%zu nodes | %u material surfaces | retained bind geometry",
                                asset.nodes.size(),asset.surfaces);
            const char* actorLabel=browser.previewActor?
                browser.previewActors[browser.previewActor-1].first.c_str():"Bind pose (raw model)";
            ImGui::SetNextItemWidth(320);
            if(ImGui::BeginCombo("Preview Actor",actorLabel)){
                if(ImGui::Selectable("Bind pose (raw model)",browser.previewActor==0)){
                    browser.previewActor=0;browser.previewPlaying=false;browser.previewTime=0;
                }
                for(std::size_t i=0;i<browser.previewActors.size();++i){
                    if(ImGui::Selectable(browser.previewActors[i].first.c_str(),browser.previewActor==i+1)){
                        browser.previewActor=i+1;browser.previewTime=0;browser.previewPlaying=false;
                    }
                }
                ImGui::EndCombo();
            }
            const ActorVisual* preview=browser.previewActor?&browser.previewActors[browser.previewActor-1].second:nullptr;
            if(preview && !preview->rigidClip.tracks.empty()){
                const auto& clip=preview->rigidClip;
                if(browser.previewPlaying){
                    browser.previewTime+=std::min(ImGui::GetIO().DeltaTime,.1f);
                    if(browser.previewTime>=clip.duration){
                        if(clip.loop)browser.previewTime=rtsClipTime(clip,browser.previewTime);
                        else{browser.previewTime=clip.duration;browser.previewPlaying=false;}
                    }
                }
                if(ImGui::SmallButton(browser.previewPlaying?"Pause":"Play")){
                    if(!browser.previewPlaying && browser.previewTime>=clip.duration)browser.previewTime=0;
                    browser.previewPlaying=!browser.previewPlaying;
                }
                ImGui::SameLine();if(ImGui::SmallButton("Rewind")){browser.previewTime=0;browser.previewPlaying=false;}
                ImGui::SameLine();ImGui::SetNextItemWidth(285);
                if(ImGui::SliderFloat("Time (s)",&browser.previewTime,0,clip.duration,"%.3f"))
                    browser.previewPlaying=false;
                ImGui::SameLine();ImGui::TextDisabled("%zu tracks | %s",clip.tracks.size(),clip.loop?"Loop":"Once");
            }
            ImGui::Separator();
            if(browser.selectedNode>=asset.nodes.size())browser.selectedNode=0;
            // Parent-first binary storage; render depth-first for the familiar
            // model browser tree, with a selectable row for each named node.
            ImGui::BeginChild("##modelNodeTree",ImVec2(210,430),true);
            if(asset.nodes.empty())ImGui::TextWrapped("Legacy mesh: no retained hierarchy.");
            const auto drawNode=[&](const auto& self,std::size_t index)->void{
                const auto& n=asset.nodes[index];
                ImGui::PushID(static_cast<int>(index));
                const std::string label=std::to_string(index)+"  "+
                    (n.name.empty()?"(unnamed)":n.name);
                if(ImGui::Selectable(label.c_str(),browser.selectedNode==index))
                    browser.selectedNode=index;
                ImGui::Indent(14.f);
                for(std::size_t child=index+1;child<asset.nodes.size();++child)
                    if(asset.nodes[child].parent==index)self(self,child);
                ImGui::Unindent(14.f);
                ImGui::PopID();
            };
            for(std::size_t n=0;n<asset.nodes.size();++n)
                if(asset.nodes[n].parent==RTS_MODEL_ROOT)drawNode(drawNode,n);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##modelViewport",ImVec2(std::max(300.f,ImGui::GetContentRegionAvail().x*.62f),430),true);
            CreatorModelPreview::viewer.draw(asset,browser.projectPath,app.assetRevision,browser.selectedNode,
                                             preview,browser.previewTime);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##nodeProperties",ImVec2(0,430),true);
            if(asset.nodes.empty())ImGui::TextWrapped("No retained nodes in this mesh.");
            else{
            const auto& node=asset.nodes[browser.selectedNode];
            ImGui::TextWrapped("%s",node.name.empty()?"(unnamed)":node.name.c_str());
            ImGui::Text("Index: %zu",browser.selectedNode);
            if(node.parent==RTS_MODEL_ROOT)ImGui::TextUnformatted("Parent: root");
            else ImGui::Text("Parent index: %u",node.parent);
            ImGui::Text("Material range: %u + %u",node.firstSurface,node.surfaceCount);
            ImGui::TextUnformatted("Local translation");
            ImGui::Text("%.3f, %.3f, %.3f",node.local[12],node.local[13],node.local[14]);
            ImGui::TextUnformatted("Model-space origin");
            ImGui::Text("%.3f, %.3f, %.3f",node.model[12],node.model[13],node.model[14]);
            ImGui::SeparatorText("Model-space basis");
            for(int r=0;r<3;++r)
                ImGui::Text("% .3f   % .3f   % .3f",node.model[r],node.model[4+r],node.model[8+r]);
            const auto unique=rtsFindUniqueModelNode(asset.nodes,node.name);
            ImGui::TextWrapped("%s",unique?"Unique named node (socket candidate)":
                               "Unnamed or ambiguous node; no unique socket lookup.");
            }
            ImGui::EndChild();
            ImGui::TextWrapped("Production mesh preview. Bounds are bind-pose bounds; selected axes follow the sampled animation.");
        }
        if(ImGui::Button("Close"))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return false;
}
} // namespace CreatorAssetLibrary
