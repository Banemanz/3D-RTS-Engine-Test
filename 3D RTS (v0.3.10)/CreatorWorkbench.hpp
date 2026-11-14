#pragma once
#include "EditorSession.hpp"
#include "RTSHUDLayout.hpp"
#include "CreatorTriggerWidgets.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>

// Dear ImGui is strictly a presentation/controller layer. The engine,
// map serialization, simulation, and undoable authoring stay headless C++.
// This same workspace is compiled into Windows and Linux native binaries.
namespace CreatorWorkbench {
template<class App>
void refresh(App& app,bool changed) {
    if(!changed)return;
    try { app.reloadFromProject(); }
    catch(const std::exception& e) {
        app.editor.message=std::string("PROJECT VALIDATION FAILED: ")+e.what();
    }
}
template<class App>
void choosePanel(App& app,EditorSession::Panel next) {
    if(app.editor.panel==next)return;
    app.editor.finishStroke(app.project);
    if(app.editor.textEditing)app.editor.cancelEdit();
    app.editor.panel=next;
    if(next==EditorSession::Panel::Play)app.reloadFromProject();
}
template<class App>
void toolbar(App& app) {
    auto& ed=app.editor;
    if(ImGui::BeginMainMenuBar()){
        if(ImGui::BeginMenu("Project")){
            if(ImGui::MenuItem("Save","Ctrl+S"))app.saveProject();
            if(ImGui::MenuItem("Undo","Ctrl+Z"))refresh(app,ed.undo(app.project));
            if(ImGui::MenuItem("Redo","Ctrl+Y"))refresh(app,ed.redo(app.project));
            ImGui::Separator();
            ImGui::TextDisabled("%s",app.projectPath.c_str());
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Workspace")){
            const EditorSession::Panel tabs[]={
                EditorSession::Panel::Play,EditorSession::Panel::Terrain,
                EditorSession::Panel::Data,EditorSession::Panel::Triggers,
                EditorSession::Panel::Scene};
            const char* labels[]={"Play","Terrain","Data","Triggers","Scene"};
            for(int i=0;i<5;++i)
                if(ImGui::MenuItem(labels[i],nullptr,ed.panel==tabs[i]))
                    choosePanel(app,tabs[i]);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View")){
            ImGui::TextDisabled("Creator GUI: docking enabled");
            ImGui::TextDisabled("Drag panel title bars to dock/float");
            ImGui::EndMenu();
        }
        ImGui::Separator();
        const EditorSession::Panel tabs[]={
            EditorSession::Panel::Play,EditorSession::Panel::Terrain,
            EditorSession::Panel::Data,EditorSession::Panel::Triggers,
            EditorSession::Panel::Scene};
        const char* names[]={"PLAY","TERRAIN","DATA","TRIGGERS","SCENE"};
        for(int i=0;i<5;++i){
            ImGui::PushID(i);
            if(ImGui::SmallButton(names[i]))choosePanel(app,tabs[i]);
            ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::Separator();
        if(ed.modified)ImGui::TextColored(ImVec4(1.f,.72f,.29f,1.f),"UNSAVED");
        else ImGui::TextColored(ImVec4(.45f,.82f,.65f,1.f),"SAVED");
        ImGui::SameLine();
        ImGui::TextDisabled("| %s",RTS_SOURCE_COMMIT);
        ImGui::EndMainMenuBar();
    }
}
template<class App>
void browser(App& app){
    auto& ed=app.editor;
    const auto& p=app.project;
    const auto w=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(12.f,50.f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f,std::max(330.f,w.y-112.f)),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Creator | Browser")){ImGui::End();return;}
    if(ed.panel==EditorSession::Panel::Terrain){
        ImGui::SeparatorText("Terrain brush");
        const EditorSession::Tool tools[]={
            EditorSession::Tool::Raise,EditorSession::Tool::Lower,
            EditorSession::Tool::Smooth,EditorSession::Tool::Flatten,
            EditorSession::Tool::Paint,EditorSession::Tool::Block,
            EditorSession::Tool::Unblock,EditorSession::Tool::PlaceMineral,
            EditorSession::Tool::PlaceUnit,EditorSession::Tool::PlaceDoodad};
        const char* names[]={"Raise","Lower","Smooth","Flatten","Paint texture",
                             "Block pathing","Clear pathing","Place mineral",
                             "Place unit","Place doodad"};
        for(int i=0;i<10;++i){
            if(ImGui::Selectable(names[i],ed.tool==tools[i]))
                ed.tool=tools[i];
        }
        ImGui::SeparatorText("Painting");
        ImGui::SliderFloat("Brush radius",&ed.brushRadius,.5f,32.f,"%.1f");
        ImGui::SliderFloat("Strength",&ed.brushStrength,.05f,5.f,"%.2f");
        ImGui::SliderInt("Material",&ed.materialId,0,7);
        ImGui::SliderInt("Owner",&ed.placementOwner,0,1);
        if(ImGui::BeginCombo("Unit",ed.placementArchetype.c_str())){
            for(const auto& e:p.catalog.entries())if(e.type=="Unit")
                if(ImGui::Selectable(e.id.c_str(),ed.placementArchetype==e.id))
                    ed.placementArchetype=e.id;
            ImGui::EndCombo();
        }
        if(ImGui::BeginCombo("Doodad actor",ed.placementDoodad.c_str())){
            for(const auto& e:p.catalog.entries())if(e.type=="Actor"||e.type=="Model")
                if(ImGui::Selectable(e.id.c_str(),ed.placementDoodad==e.id))
                    ed.placementDoodad=e.id;
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("Left-click the terrain to paint or place objects. Right-click removes nearby placements.");
    }else if(ed.panel==EditorSession::Panel::Data){
        ImGui::SeparatorText("Data catalog");
        // Data-family selection is independent of the search string, so
        // custom mod-defined catalog classes work without hardcoded UI tabs.
        const std::string category=ed.catalogTypeFilter.empty()?
            "All object types":ed.catalogTypeFilter;
        ImGui::SetNextItemWidth(-1.f);
        if(ImGui::BeginCombo("##type",category.c_str())){
            if(ImGui::Selectable("All object types",ed.catalogTypeFilter.empty()))
                ed.setCatalogTypeFilter(p,"");
            std::set<std::string> types;
            for(const auto& entry:p.catalog.entries())types.insert(entry.type);
            for(const auto& type:types)
                if(ImGui::Selectable(type.c_str(),ed.catalogTypeFilter==type))
                    ed.setCatalogTypeFilter(p,type);
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##find","Search IDs, types or names",&ed.catalogFilter);
        ImGui::TextDisabled("%zu of %zu objects",
            ed.matchingCatalogCount(p),p.catalog.entries().size());
        if(ImGui::SmallButton("Clear search"))ed.clearCatalogSearch(p);
        ImGui::SameLine();
        if(ImGui::SmallButton("Derive selected"))
            refresh(app,ed.duplicateCatalogObject(app.project));
        ImGui::BeginChild("##catalogTree",ImVec2(0,-5),true);
        const auto& entries=p.catalog.entries();
        std::vector<std::size_t> visible;
        visible.reserve(ed.matchingCatalogCount(p));
        for(std::size_t i=0;i<entries.size();++i)
            if(ed.catalogMatches(entries[i]))visible.push_back(i);
        // UI work is proportional to rows on screen, not all 10k entries.
        // Filtering and indexing stay in headless C++.
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(visible.size()));
        while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row){
            const std::size_t i=visible[static_cast<std::size_t>(row)];
            ImGui::PushID(static_cast<int>(i));
            const bool chosen=i==ed.selectedObject;
            const std::string label=entries[i].type+" / "+entries[i].id;
            if(ImGui::Selectable(label.c_str(),chosen)){
                ed.selectedObject=i;ed.selectedField=0;ed.cancelEdit();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }else if(ed.panel==EditorSession::Panel::Scene){
        ImGui::SeparatorText("Scene hierarchy");
        if(ImGui::SmallButton("Point")){
            refresh(app,ed.placeSceneMarker(app.project,app.camera.focalPoint,false));
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("Region")){
            refresh(app,ed.placeSceneMarker(app.project,app.camera.focalPoint,true));
        }
        ImGui::Separator();
        ImGui::TextDisabled("%zu scene instances",p.placements.size());
        ImGui::BeginChild("##sceneTree",ImVec2(0,0),true);
        ImGuiListClipper sceneClip;
        sceneClip.Begin(static_cast<int>(p.placements.size()));
        while(sceneClip.Step())for(int i=sceneClip.DisplayStart;i<sceneClip.DisplayEnd;++i){
            const auto& item=p.placements[static_cast<std::size_t>(i)];
            const std::string name=item.kind+" | "+item.id;
            if(ImGui::Selectable(name.c_str(),ed.selectedPlacementId==item.id))
                ed.selectedPlacementId=item.id;
        }
        ImGui::EndChild();
    }else if(ed.panel==EditorSession::Panel::Triggers){
        ImGui::SeparatorText("Events / Conditions / Actions");
        if(ImGui::Button("New trigger"))
            refresh(app,ed.createTrigger(app.project));
        ImGui::SameLine();
        if(ImGui::Button("Duplicate"))
            refresh(app,ed.duplicateTrigger(app.project));
        ImGui::SameLine();
        if(ImGui::Button("Delete"))
            refresh(app,ed.deleteTrigger(app.project));
        ImGui::Separator();
        ImGui::BeginChild("##triggers",ImVec2(0,0),true);
        for(std::size_t i=0;i<ed.triggerCount(p);++i){
            ImGui::PushID(static_cast<int>(i));
            if(ImGui::Selectable(ed.triggerLabel(p,i).c_str(),ed.selectedTrigger==i)){
                ed.selectedTrigger=i;ed.cancelEdit();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
template<class App>
bool navigateCatalog(App& app,const char* role,const std::string& id) {
    if(id.empty() || !app.project.catalog.find(id))return false;
    const std::string label=std::string(role)+": "+id;
    if(!ImGui::SmallButton(label.c_str()))return false;
    return app.editor.selectCatalogId(app.project,id);
}
template<class App>
void inspector(App& app){
    auto& ed=app.editor;
    auto& project=app.project;
    const auto w=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(std::max(320.f,w.x-375.f),50.f),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.f,std::max(330.f,w.y-112.f)),
                             ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Creator | Inspector")){ImGui::End();return;}
    ImGui::TextDisabled("PROJECT  %s",project.name.c_str());
    ImGui::TextDisabled("MAP  %d x %d",project.terrain.width(),project.terrain.depth());
    ImGui::Separator();
    if(ed.panel==EditorSession::Panel::Terrain){
        ImGui::Text("Tool: %s",ed.toolName().c_str());
        ImGui::Text("Instances: %zu",project.placements.size());
        if(app.hoveredTerrain){
            const Vec3& pos=app.hoveredWorld;
            ImGui::Text("Cursor   X %.2f   Y %.2f   Z %.2f",pos.x,pos.y,pos.z);
        }
        if(ImGui::Button("Undo stroke"))refresh(app,ed.undo(project));
        ImGui::SameLine();
        if(ImGui::Button("Redo stroke"))refresh(app,ed.redo(project));
        ImGui::Separator();
        ImGui::TextWrapped("Scene placements, actors, data and triggers are editable in their own workspace tabs.");
    }else if(ed.panel==EditorSession::Panel::Data){
        const CatalogEntry* entry=ed.selected(project);
        if(entry){
            ImGui::Text("Type: %s",entry->type.c_str());
            ImGui::TextWrapped("ID: %s",entry->id.c_str());
            ImGui::TextWrapped("Parent: %s",entry->parent.empty()?"(root)":entry->parent.c_str());
            if(!ed.textEditing){
                if(ImGui::Button("Rename"))ed.beginCatalogRename(project);
                ImGui::SameLine();
                if(ImGui::Button("Remove object")){
                    refresh(app,ed.deleteCatalogObject(project));
                    ImGui::End();return; // deleted catalog entry invalidates pointers
                }
                // SC2-style object explorer: links are typed, inherited,
                // actionable, and resolved through the headless project data.
                const auto resolved=project.catalog.resolvedFields(entry->id);
                ImGui::SeparatorText("Object links");
                if(!entry->parent.empty() && navigateCatalog(app,"Parent",entry->parent)){
                    ImGui::End();return;
                }
                // Explicit branches keep MSVC/GCC template name lookup
                // identical (nested captures of templated App can diverge).
                const auto actorLink=resolved.find("actor");
                if(entry->type=="Unit" && actorLink!=resolved.end() &&
                   actorLink->second.template is<std::string>() &&
                   navigateCatalog(app,"Actor",actorLink->second.template get<std::string>())){
                    ImGui::End();return;
                }
                const auto modelLink=resolved.find("model");
                if(entry->type=="Actor" && modelLink!=resolved.end() &&
                   modelLink->second.template is<std::string>() &&
                   navigateCatalog(app,"Model",modelLink->second.template get<std::string>())){
                    ImGui::End();return;
                }
                if(entry->type=="Unit"){
                    auto training=resolved.find("trainable_units");
                    if(training!=resolved.end() && training->second.template is<picojson::array>()){
                        for(const auto& value:training->second.template get<picojson::array>()){
                            if(value.template is<std::string>() &&
                               navigateCatalog(app,"Trains",value.template get<std::string>())){
                                ImGui::End();return;
                            }
                        }
                    }
                }
                if(entry->type=="Effect"){
                    const auto kind=resolved.find("kind");
                    if(kind!=resolved.end() &&
                       kind->second.template is<std::string>() &&
                       kind->second.template get<std::string>()=="Sequence"){
                        const auto members=resolved.find("children");
                        if(members!=resolved.end() &&
                           members->second.template is<picojson::array>()){
                            const auto children=members->second.template get<picojson::array>();
                            ImGui::SeparatorText("Effect graph | Sequence");
                            ImGui::TextDisabled("%zu ordered effects (maximum 8)",children.size());
                            for(std::size_t i=0;i<children.size();++i){
                                if(!children[i].template is<std::string>())continue;
                                ImGui::PushID(static_cast<int>(i));
                                const std::string refId=children[i].template get<std::string>();
                                if(navigateCatalog(app,"Open effect",refId)){
                                    ImGui::PopID();ImGui::End();return;
                                }
                                if(children.size()>1){
                                    ImGui::SameLine();
                                    if(ImGui::SmallButton("Remove")){
                                        auto changed=children;
                                        changed.erase(changed.begin()+static_cast<std::ptrdiff_t>(i));
                                        refresh(app,ed.setCatalogField(project,"children",
                                            picojson::value(changed)));
                                        ImGui::PopID();ImGui::End();return;
                                    }
                                }
                                ImGui::PopID();
                            }
                            if(children.size()<8 &&
                               ImGui::BeginCombo("Append effect","Choose Effect...")){
                                for(const auto& candidate:project.catalog.entries())
                                    if(candidate.type=="Effect" && candidate.id!=entry->id){
                                        if(ImGui::Selectable(candidate.id.c_str(),false)){
                                            auto changed=children;
                                            changed.emplace_back(candidate.id);
                                            refresh(app,ed.setCatalogField(project,"children",
                                                picojson::value(changed)));
                                            ImGui::EndCombo();ImGui::End();return;
                                        }
                                    }
                                ImGui::EndCombo();
                            }
                        }
                    }
                }
                const auto incoming=project.catalogReferences(entry->id);
                if(!incoming.empty()){
                    if(ImGui::TreeNode("##uses","Used by (%zu)",incoming.size())){
                        for(std::size_t i=0;i<std::min<std::size_t>(incoming.size(),24);++i)
                            ImGui::TextWrapped("%s",incoming[i].c_str());
                        if(incoming.size()>24)
                            ImGui::TextDisabled("... %zu more",incoming.size()-24);
                        ImGui::TreePop();
                    }
                }
                ImGui::SeparatorText("Resolved properties");
                const auto fields=ed.fields(project);
                ImGui::BeginChild("##fields",ImVec2(0,210.f),true);
                for(std::size_t i=0;i<fields.size();++i){
                    const auto& name=fields[i];
                    const std::string display=(entry->fields.count(name)?"[LOCAL] ":"[INHERITED] ")+
                                              name+" = "+resolved.at(name).serialize();
                    if(ImGui::Selectable(display.c_str(),ed.selectedField==i)){
                        ed.selectedField=i;
                        if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            ed.beginValueEdit(project);
                    }
                }
                ImGui::EndChild();
                if(!ed.textEditing && !fields.empty()){
                    const std::string field=ed.selectedFieldName(project);
                    auto found=resolved.find(field);
                    if(found!=resolved.end()){
                        ImGui::TextWrapped("Selected: %s",field.c_str());
                        // Widgets edit actual catalog data through one
                        // validated/undoable C++ mutation, never raw pointers.
                        static std::string draftIdentity;
                        static double draftNumber=0.;
                        static std::string draftString;
                        const std::string identity=entry->id+"."+field;
                        if(identity!=draftIdentity){
                            draftIdentity=identity;
                            if(found->second.template is<double>())
                                draftNumber=found->second.template get<double>();
                            if(found->second.template is<std::string>())
                                draftString=found->second.template get<std::string>();
                        }
                        if(found->second.template is<bool>()){
                            bool enabled=found->second.template get<bool>();
                            if(ImGui::Checkbox("Value",&enabled))
                                refresh(app,ed.setCatalogField(project,field,
                                    picojson::value(enabled)));
                        }else if(found->second.template is<double>()){
                            ImGui::InputDouble("Value",&draftNumber,0.,0.,"%.3f");
                            if(ImGui::IsItemDeactivatedAfterEdit()){
                                refresh(app,ed.setCatalogField(project,field,
                                    picojson::value(draftNumber)));
                                draftNumber=project.catalog.resolvedFields(entry->id).at(field).template get<double>();
                            }
                        }else if(found->second.template is<std::string>()){
                            ImGui::InputText("Value",&draftString);
                            if(ImGui::IsItemDeactivatedAfterEdit()){
                                refresh(app,ed.setCatalogField(project,field,
                                    picojson::value(draftString)));
                                draftString=project.catalog.resolvedFields(entry->id).at(field).template get<std::string>();
                            }
                        }else ImGui::TextDisabled("Array/object: edit as JSON");
                    }
                }
                if(ImGui::Button("Edit property JSON"))ed.beginValueEdit(project);
                ImGui::SameLine();
                if(ImGui::Button("Add property"))ed.beginNewField();
                if(ImGui::Button("Reset to inherited"))
                    refresh(app,ed.removeLocalOverride(project));
            }
        }
    }else if(ed.panel==EditorSession::Panel::Scene){
        const auto* item=ed.selectedPlacement(project);
        if(item){
            // Command mutations may grow/replace project. Never use a
            // pointer into placements after reloading the simulation.
            const ProjectPlacement copy=*item;
            ImGui::Text("ID: %s",copy.id.c_str());
            ImGui::Text("Kind: %s",copy.kind.c_str());
            ImGui::TextWrapped("Catalog: %s",copy.catalogId.c_str());
            if(!copy.catalogId.empty() && project.catalog.find(copy.catalogId) &&
               ImGui::SmallButton("Open linked catalog entry")){
                ed.panel=EditorSession::Panel::Data;
                ed.selectCatalogId(project,copy.catalogId);
                ImGui::End();return;
            }
            ImGui::Text("Owner: %d",copy.owner);
            if(ImGui::Button("Switch owner")){
                refresh(app,ed.toggleSelectedOwner(project));
                ImGui::End();return;
            }
            ImGui::SameLine();
            if(ImGui::Button("Duplicate")){
                refresh(app,ed.duplicateSelectedPlacement(project));
                ImGui::End();return;
            }
            ImGui::SameLine();
            if(ImGui::Button("Delete")){
                refresh(app,ed.deleteSelectedPlacement(project));
                ImGui::End();return;
            }
            // Edit drafts persist across frames. ImGui deliberately rejects
            // EnterReturnsTrue for numeric InputScalar (Debug builds assert);
            // commit only once on focus loss, keeping a single undo snapshot.
            // Do not reset active drafts from the document every frame.
            static std::string draftId;
            static float draftPos[3]={0.f,0.f,0.f};
            static float draftYaw=0.f;
            if(draftId!=copy.id){
                draftId=copy.id;
                draftPos[0]=copy.position.x;
                draftPos[1]=copy.position.y;
                draftPos[2]=copy.position.z;
                draftYaw=copy.rotation.y;
            }
            bool committedPosition=false;
            ImGui::TextUnformatted("Position");
            ImGui::PushID(copy.id.c_str());
            ImGui::InputFloat("X",&draftPos[0],0.f,0.f,"%.2f");
            committedPosition|=ImGui::IsItemDeactivatedAfterEdit();
            ImGui::InputFloat("Y",&draftPos[1],0.f,0.f,"%.2f");
            committedPosition|=ImGui::IsItemDeactivatedAfterEdit();
            ImGui::InputFloat("Z",&draftPos[2],0.f,0.f,"%.2f");
            committedPosition|=ImGui::IsItemDeactivatedAfterEdit();
            if(committedPosition){
                refresh(app,ed.moveSelectedPlacement(project,
                    Vec3(draftPos[0],draftPos[1],draftPos[2])));
                // Validation may reject the edit. Always mirror the actual
                // document afterward rather than preserving invalid drafts.
                if(const auto* updated=ed.selectedPlacement(project)){
                    draftPos[0]=updated->position.x;
                    draftPos[1]=updated->position.y;
                    draftPos[2]=updated->position.z;
                }
            }
            ImGui::InputFloat("Yaw",&draftYaw,0.f,0.f,"%.1f");
            if(ImGui::IsItemDeactivatedAfterEdit()){
                refresh(app,ed.rotateSelectedPlacement(project,
                    draftYaw-copy.rotation.y));
                if(const auto* updated=ed.selectedPlacement(project))
                    draftYaw=updated->rotation.y;
            }
            ImGui::PopID();
            if(ImGui::Button("Scale -"))
                refresh(app,ed.scaleSelectedPlacement(project,.9f));
            ImGui::SameLine();
            if(ImGui::Button("Scale +"))
                refresh(app,ed.scaleSelectedPlacement(project,1.1f));
            if(copy.kind=="Region"){
                if(ImGui::Button("Radius -"))
                    refresh(app,ed.adjustSelectedRegionRadius(project,-.5f));
                ImGui::SameLine();
                if(ImGui::Button("Radius +"))
                    refresh(app,ed.adjustSelectedRegionRadius(project,.5f));
            }
            if(ImGui::Button("Edit instance JSON"))
                ed.beginSceneOverrideEdit(project);
        }else ImGui::TextWrapped("Select an object in the hierarchy or click it in the world viewport.");
    }else if(ed.panel==EditorSession::Panel::Triggers){
        CreatorTriggerWidgets::draw(app);
    }
    if(ed.textEditing){
        ImGui::SeparatorText("Edit");
        ImGui::TextWrapped("%s",ed.message.c_str());
        ImGui::InputTextMultiline("##json",&ed.editText,ImVec2(-1.f,160.f));
        if(ImGui::Button("Apply")){
            if(ed.filterEditing)ed.finishCatalogSearch(project);
            else if(ed.renamingCatalog)
                refresh(app,ed.finishCatalogRename(project));
            else if(ed.panel==EditorSession::Panel::Data)
                refresh(app,ed.finishEdit(project));
            else if(ed.panel==EditorSession::Panel::Scene)
                refresh(app,ed.finishSceneOverrideEdit(project));
            else if(ed.panel==EditorSession::Panel::Triggers)
                refresh(app,ed.finishTriggerEdit(project));
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel edit"))ed.cancelEdit();
    }
    if(!ed.message.empty()){
        ImGui::SeparatorText("Messages");
        ImGui::TextWrapped("%s",ed.message.c_str());
    }
    ImGui::End();
}
template<class App>
void draw(App& app){
    if(app.pauseMenu)return; // Native pause UI owns the entire overlay/input.
    toolbar(app);
    if(app.editor.panel==EditorSession::Panel::Play)return;
    // The center is a pass-through 3D viewport. Panels can dock around it
    // without forcing world clicks to be interpreted as GUI input.
    ImGui::DockSpaceOverViewport(0,ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    browser(app);
    inspector(app);
}
} // namespace CreatorWorkbench
