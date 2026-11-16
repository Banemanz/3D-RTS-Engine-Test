#pragma once
#include "EditorSession.hpp"
#include "RTSHUDLayout.hpp"
#include "CreatorTriggerWidgets.hpp"
#include "CreatorReferenceWidgets.hpp"
#include "CreatorEffectGraphWidgets.hpp"
#include "CreatorAreaShapePreview.hpp"
#include "CreatorCatalogNavigator.hpp"
#include "CreatorTechGraph.hpp"
#include "CreatorAIResearch.hpp"
#include "CreatorUISettings.hpp"
#include "RTSCatalogValuePreview.hpp"
#include "CreatorTriggerMonitor.hpp"
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
inline bool modulePane=false;
inline void endPane(){if(modulePane)ImGui::EndChild();else ImGui::End();}
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
            if(ImGui::MenuItem("Export playtest replay", "Ctrl+Shift+L",
                  false,app.editor.panel==EditorSession::Panel::Play))
                app.saveReplay();
            ImGui::Separator();
            ImGui::MenuItem("Playtest trigger monitor",nullptr,&app.triggerTraceVisible);
            ImGui::MenuItem("Data relationship navigator",nullptr,
                            &CreatorCatalogNavigator::visible);
            ImGui::MenuItem("Technology dependency map",nullptr,
                            &CreatorTechGraph::visible);
            ImGui::MenuItem("Opponent research planner",nullptr,
                            &CreatorAIResearch::visible);
            ImGui::Separator();
            ImGui::TextDisabled("Creator GUI: docking enabled");
            ImGui::TextDisabled("Drag panel title bars to dock/float");
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Appearance")){
            auto& prefs=app.uiSettings;
            bool changed=false;
            float value=static_cast<float>(prefs.uiScale);
            ImGui::SetNextItemWidth(240.f);
            if(ImGui::SliderFloat("UI scale",&value,.85f,2.50f,"%.2fx")){
                prefs.uiScale=std::round(double(value)*100.)/100.;
                changed=true;
            }
            ImGui::TextDisabled("Manual: %.0f%% | effective: %.0f%%",
                prefs.uiScale*100.,app.uiEffectiveScale*100.f);
            const double presets[]={1.,1.25,1.50,1.75,2.,2.5};
            const char* names[]={"100%","125%","150%","175%","200%","250%"};
            for(int i=0;i<6;++i){
                if(i%3!=0)ImGui::SameLine();
                if(ImGui::SmallButton(names[i])){
                    prefs.uiScale=presets[i];changed=true;
                }
            }
            changed|=ImGui::Checkbox("Follow OS / monitor DPI",
                                     &prefs.followSystemDpi);
            changed|=ImGui::Checkbox("Enhanced text contrast",
                                     &prefs.enhancedContrast);
            changed|=ImGui::Checkbox("Separate editor modules",
                                     &prefs.separateModules);
            ImGui::Separator();
            if(ImGui::MenuItem("Reset editor appearance")){
                prefs=CreatorUISettings{};changed=true;
            }
            ImGui::TextWrapped("Text and controls are resized together. These preferences are per-user, never saved into a map.");
            if(changed)app.uiSettingsDirty=true;
            ImGui::EndMenu();
        }
        ImGui::Separator();
        const EditorSession::Panel tabs[]={
            EditorSession::Panel::Play,EditorSession::Panel::Terrain,
            EditorSession::Panel::Data,EditorSession::Panel::Triggers,
            EditorSession::Panel::Scene};
        const char* names[]={"PLAY","TERRAIN","DATA","TRIGGERS","SCENE"};
        if(ImGui::GetContentRegionAvail().x>565.f)
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
        const std::string shortCommit=std::string(RTS_SOURCE_COMMIT).substr(0,8);
        if(ImGui::GetContentRegionAvail().x>120.f)
            ImGui::TextDisabled("| %s",shortCommit.c_str());
        ImGui::EndMainMenuBar();
    }
}
template<class App>
void browser(App& app){
    auto& ed=app.editor;
    const auto& p=app.project;
    const auto w=ImGui::GetIO().DisplaySize;
    if(modulePane){
        if(!ImGui::BeginChild("##moduleBrowser",ImVec2(0,0),
                              ImGuiChildFlags_Borders)){ImGui::EndChild();return;}
        ImGui::TextDisabled("NAVIGATOR");
        ImGui::Separator();
    }else{
        const float unit=ImGui::GetFontSize()/16.5f;
        ImGui::SetNextWindowPos(ImVec2(12.f*unit,50.f*unit),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.f*unit,
            std::max(330.f*unit,w.y-76.f*unit)),ImGuiCond_FirstUseEver);
        if(!ImGui::Begin("Creator | Browser")){ImGui::End();return;}
    }
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
        if(ImGui::BeginCombo("##createCatalogObject","New catalog object...")){
            struct PresetChoice {const char* type;const char* preset;const char* label;};
            const PresetChoice presets[]={
                {"Unit","Combat","Unit | Combat"},
                {"Unit","Worker","Unit | Worker"},
                {"Unit","Structure","Unit | Structure"},
                {"Validator","Health","Validator | Health threshold"},
                {"Validator","Energy","Validator | Energy threshold"},
                {"Validator","Status","Validator | Status presence"},
                {"Validator","Structure","Validator | Is structure"},
                {"Validator","UnitCount","Validator | Owned unit count"},
                {"Validator","Upgrade","Validator | Research level"},
                {"Requirement","Predicate","Requirement | Single validator"},
                {"Requirement","All","Requirement | All children"},
                {"Requirement","Any","Requirement | Any child"},
                {"Requirement","Not","Requirement | Negate child"},
                {"Upgrade","Damage","Upgrade | Attack research"},
                {"Upgrade","Armor","Upgrade | Armor research"},
                {"Weapon","Ranged","Weapon | Ranged rifle"},
                {"Weapon","Melee","Weapon | Melee blade"},
                {"Projectile","Arc","Projectile | Arcing spark"},
                {"Projectile","Bolt","Projectile | Intercepting bolt"},
                {"Behavior","Fortify","Behavior | Armor shield"},
                {"Behavior","Sharpen","Behavior | Attack focus"},
                {"Behavior","Hinder","Behavior | Movement slow"},
                {"Ability","Self","Ability | Self"},
                {"Ability","Ally","Ability | Ally target"},
                {"Ability","Enemy","Ability | Enemy target"},
                {"Ability","Any","Ability | Any unit target"},
                {"Ability","Ground","Ability | Ground area impact"},
                {"Actor","Visual","Actor | Visual"},
                {"Actor","Events","Actor | Event-driven visual"},
                {"Model","Procedural","Model | Procedural"},
                {"Effect","Damage","Effect | Damage"},
                {"Effect","Heal","Effect | Heal"},
                {"Effect","PeriodicDamage","Effect | Periodic Damage"},
                {"Effect","PeriodicHeal","Effect | Periodic Heal"},
                {"Effect","ApplyBehavior","Effect | Apply Behavior"},
                {"Effect","RemoveBehavior","Effect | Remove Behavior"},
                {"Effect","Sequence","Effect | Sequence"}
            };
            for(const auto& item:presets)
                if(ImGui::Selectable(item.label))
                    refresh(app,ed.createCatalogObject(app.project,item.type,item.preset));
            ImGui::EndCombo();
        }

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
    endPane();
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
    // The F7 catalog contains grouped status controls and nested Effect
    // graph nodes. Give it a real work area rather than clipping labels at
    // 360px. Default geometry alone changes; user-resized/docked windows
    // remain authoritative on every OS.
    const float inspectorWidth=ed.panel==EditorSession::Panel::Triggers?490.f:
        ed.panel==EditorSession::Panel::Data?
            (CreatorAIResearch::visible ? 360.f :
             std::clamp(w.x*.40f,400.f,540.f)):360.f;
    if(modulePane){
        if(!ImGui::BeginChild("##moduleInspector",ImVec2(0,0),
                              ImGuiChildFlags_Borders)){ImGui::EndChild();return;}
        ImGui::TextDisabled("PROPERTIES / DETAILS");
    }else{
        const float unit=ImGui::GetFontSize()/16.5f;
        ImGui::SetNextWindowPos(
            ImVec2(std::max(320.f*unit,w.x-inspectorWidth*unit-15.f*unit),
                   50.f*unit),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(inspectorWidth*unit,std::max(330.f*unit,w.y-76.f*unit)),
            ImGuiCond_FirstUseEver);
        if(!ImGui::Begin("Creator | Inspector")){endPane();return;}
    }
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
                    endPane();return; // deleted catalog entry invalidates pointers
                }
                // SC2-style object explorer: links are typed, inherited,
                // actionable, and resolved through the headless project data.
                const auto resolved=project.catalog.resolvedFields(entry->id);
                ImGui::SeparatorText("Object links");
                if(!entry->parent.empty() && navigateCatalog(app,"Parent",entry->parent)){
                    endPane();return;
                }
                // Explicit branches keep MSVC/GCC template name lookup
                // identical (nested captures of templated App can diverge).
                const auto actorLink=resolved.find("actor");
                if(entry->type=="Unit" && actorLink!=resolved.end() &&
                   actorLink->second.template is<std::string>() &&
                   navigateCatalog(app,"Actor",actorLink->second.template get<std::string>())){
                    endPane();return;
                }
                const auto modelLink=resolved.find("model");
                if(entry->type=="Actor" && modelLink!=resolved.end() &&
                   modelLink->second.template is<std::string>() &&
                   navigateCatalog(app,"Model",modelLink->second.template get<std::string>())){
                    endPane();return;
                }
                if(entry->type=="Actor"){
                    ImGui::SeparatorText("Actor event rules | Presentation only");
                    static std::string editingActorCues;
                    static std::vector<RTSActorCueDefinition> draftCues;
                    if(editingActorCues!=entry->id){
                        draftCues=compileActorCues(project.catalog,entry->id);
                        editingActorCues=entry->id;
                    }
                    ImGui::TextWrapped("Events react to actual fixed-tick gameplay. Their colors and shapes never change gameplay damage, collisions or replay outcomes.");
                    ImGui::TextDisabled("%zu / 12 rules | 1..180 ticks at 60Hz",
                                        draftCues.size());
                    if(draftCues.size()<12 && ImGui::SmallButton("Add actor event"))
                        draftCues.emplace_back();
                    const char* events[]={"Attack","Launch","Impact","Death",
                                          "Cast","Status"};
                    const char* styles[]={"Pulse","Ring","Beacon"};
                    for(std::size_t i=0;i<draftCues.size();++i){
                        ImGui::PushID(static_cast<int>(i));
                        auto& cue=draftCues[i];
                        ImGui::SeparatorText(("Cue "+std::to_string(i+1)).c_str());
                        int kind=static_cast<int>(cue.event);
                        if(ImGui::Combo("On gameplay event",&kind,events,6))
                            cue.event=static_cast<RTSActorCueEvent>(kind);
                        int style=static_cast<int>(cue.style);
                        if(ImGui::Combo("Visual style",&style,styles,3))
                            cue.style=static_cast<RTSActorCueStyle>(style);
                        float tint[3]={cue.tint.x,cue.tint.y,cue.tint.z};
                        if(ImGui::ColorEdit3("Cue color",tint))
                            cue.tint=Vec3(tint[0],tint[1],tint[2]);
                        ImGui::InputFloat("Cue radius",&cue.radius,.25f,1.f,"%.2f");
                        int duration=int(cue.durationTicks);
                        if(ImGui::InputInt("Duration (ticks)",&duration,5,20))
                            cue.durationTicks=static_cast<std::uint32_t>(
                                std::max(0,duration));
                        if(ImGui::SmallButton("Delete cue")){
                            draftCues.erase(draftCues.begin()+
                                static_cast<std::ptrdiff_t>(i));
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    if(ImGui::Button("Apply actor events")){
                        if(ed.setActorCues(project,draftCues)){
                            refresh(app,true);editingActorCues.clear();
                            endPane();return;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Revert actor event draft"))
                        draftCues=compileActorCues(project.catalog,entry->id);
                    ImGui::TextDisabled("Radius .25..12 | Duplicate event types allowed");
                    ImGui::Separator();
                }
                if(entry->type=="Upgrade"){
                    ImGui::SeparatorText("Owner research | Persistent tech");
                    static RTSUpgradeDefinition draft;
                    static std::string editingUpgrade;
                    if(editingUpgrade!=entry->id){
                        draft=compileUpgrade(project.catalog,entry->id);
                        editingUpgrade=entry->id;
                    }
                    if(ImGui::InputInt("Maximum research level",&draft.maxLevel) &&
                       draft.maxLevel>=1 && draft.maxLevel<=5)
                        draft.levels.resize(static_cast<std::size_t>(draft.maxLevel),
                            RTSUpgradeLevel{draft.mineralCost,draft.researchTicks,
                                            draft.damagePerLevel,draft.armorPerLevel});
                    ImGui::InputInt("Minerals per level",&draft.mineralCost,10,50);
                    int ticks=int(draft.researchTicks);
                    if(ImGui::InputInt("Research time (ticks)",&ticks,30,60))
                        draft.researchTicks=static_cast<std::uint32_t>(
                            std::max(0,ticks));
                    ImGui::InputFloat("Damage / level",&draft.damagePerLevel,.5f,2.f,"%.2f");
                    ImGui::InputFloat("Armor / level",&draft.armorPerLevel,.5f,2.f,"%.2f");
                    ImGui::SeparatorText("Research level progression");
                    if(ImGui::Checkbox("Customize individual levels",&draft.customLevels) &&
                       draft.customLevels && draft.maxLevel>=1 &&
                       draft.maxLevel<=5)
                        draft.levels.assign(static_cast<std::size_t>(draft.maxLevel),
                            RTSUpgradeLevel{draft.mineralCost,draft.researchTicks,
                                            draft.damagePerLevel,draft.armorPerLevel});
                    if(draft.customLevels){
                        ImGui::TextWrapped("Each row sets the cost, duration and incremental stat gain upon completing that level.");
                        for(std::size_t i=0;i<draft.levels.size() && i<5;++i){
                            ImGui::PushID(static_cast<int>(i));
                            if(ImGui::TreeNodeEx("level",ImGuiTreeNodeFlags_DefaultOpen,
                                                 "Level %d",static_cast<int>(i+1))){
                                auto& row=draft.levels[i];
                                ImGui::InputInt("Mineral cost",&row.mineralCost,10,50);
                                int duration=static_cast<int>(row.researchTicks);
                                if(ImGui::InputInt("Duration (ticks)",&duration,30,60))
                                    row.researchTicks=static_cast<std::uint32_t>(
                                        std::max(0,duration));
                                ImGui::InputFloat("Damage gain",&row.damageBonus,.5f,2.f,"%.2f");
                                ImGui::InputFloat("Armor gain",&row.armorBonus,.5f,2.f,"%.2f");
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                    }
                    const std::string target=draft.unitId.empty()?
                        "All owned Unit types":draft.unitId;
                    if(ImGui::BeginCombo("Affected Unit class",target.c_str())){
                        if(ImGui::Selectable("All owned Unit types",
                                             draft.unitId.empty()))
                            draft.unitId.clear();
                        for(const auto& item:project.catalog.entries())
                            if(item.type=="Unit" &&
                               ImGui::Selectable(item.id.c_str(),draft.unitId==item.id))
                                draft.unitId=item.id;
                        ImGui::EndCombo();
                    }
                    const std::string show=draft.showRequirementId.empty()?
                        "Always visible":draft.showRequirementId;
                    if(ImGui::BeginCombo("Show tech when",show.c_str())){
                        if(ImGui::Selectable("Always visible",draft.showRequirementId.empty()))
                            draft.showRequirementId.clear();
                        for(const auto& item:project.catalog.entries())
                            if(item.type=="Requirement" &&
                               ImGui::Selectable(item.id.c_str(),
                                                 draft.showRequirementId==item.id))
                                draft.showRequirementId=item.id;
                        ImGui::EndCombo();
                    }
                    if(!draft.showRequirementId.empty() &&
                       navigateCatalog(app,"Open Show graph",draft.showRequirementId)){
                        endPane();return;
                    }
                    const std::string req=draft.requirementId.empty()?
                        "None (unrestricted)":draft.requirementId;
                    if(ImGui::BeginCombo("Use tech when",req.c_str())){
                        if(ImGui::Selectable("None (unrestricted)",
                                             draft.requirementId.empty()))
                            draft.requirementId.clear();
                        for(const auto& item:project.catalog.entries())
                            if(item.type=="Requirement" &&
                               ImGui::Selectable(item.id.c_str(),
                                                 draft.requirementId==item.id))
                                draft.requirementId=item.id;
                        ImGui::EndCombo();
                    }
                    if(!draft.requirementId.empty() &&
                       navigateCatalog(app,"Open tech prerequisite",draft.requirementId)){
                        endPane();return;
                    }
                    ImGui::TextWrapped("Show Requirement hides a command until its Caster rule passes. Use Requirement leaves it visible but disabled until its prerequisite passes. Completion affects present and future owned Units.");
                    if(ImGui::Button("Apply Upgrade research")){
                        if(ed.setUpgradeDefinition(project,draft)){
                            refresh(app,true);editingUpgrade.clear();
                            endPane();return;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Revert Upgrade"))
                        draft=compileUpgrade(project.catalog,entry->id);
                    ImGui::TextDisabled("Levels 1..5 | Minerals 0..100000 | Ticks 1..36000");
                    ImGui::TextDisabled("Damage/Armor per level 0..100 | Unit bonus cap 1000");
                }
                if(entry->type=="Validator"){
                    ImGui::SeparatorText("Reusable Validator | Gameplay");
                    static RTSValidatorDefinition draft;
                    static std::string editingValidator;
                    if(editingValidator!=entry->id){
                        draft=compileValidator(project.catalog,entry->id);
                        editingValidator=entry->id;
                    }
                    const char* kinds[]={"HealthBelowPercent","EnergyAtLeast",
                        "HasBehavior","IsStructure","OwnerUnitCountAtLeast",
                        "UpgradeLevelAtLeast"};
                    int kind=static_cast<int>(draft.kind);
                    if(ImGui::Combo("Predicate kind",&kind,kinds,6))
                        draft.kind=static_cast<RTSValidatorKind>(kind);
                    const char* subjects[]={"Caster","Target"};
                    int subject=static_cast<int>(draft.subject);
                    if(ImGui::Combo("Read actor",&subject,subjects,2))
                        draft.subject=static_cast<RTSValidatorSubject>(subject);
                    if(draft.kind==RTSValidatorKind::HealthBelowPercent)
                        ImGui::InputFloat("Below HP %",&draft.value,.5f,5.f,"%.1f");
                    if(draft.kind==RTSValidatorKind::EnergyAtLeast)
                        ImGui::InputFloat("Minimum energy",&draft.value,1.f,5.f,"%.1f");
                    if(draft.kind==RTSValidatorKind::UpgradeLevelAtLeast){
                        ImGui::InputFloat("Required Upgrade level",&draft.value,1.f,2.f,"%.0f");
                        const std::string chosen=draft.upgradeId.empty()?
                            "Choose Upgrade":draft.upgradeId;
                        if(ImGui::BeginCombo("Required Upgrade",chosen.c_str())){
                            for(const auto& option:project.catalog.entries())
                                if(option.type=="Upgrade" &&
                                   ImGui::Selectable(option.id.c_str(),
                                                      option.id==draft.upgradeId))
                                    draft.upgradeId=option.id;
                            ImGui::EndCombo();
                        }
                        if(!draft.upgradeId.empty() &&
                           navigateCatalog(app,"Open Upgrade",draft.upgradeId)){
                            endPane();return;
                        }
                    }
                    if(draft.kind==RTSValidatorKind::OwnerUnitCountAtLeast){
                        ImGui::InputFloat("Minimum owned Units",&draft.value,1.f,5.f,"%.0f");
                        const std::string current=draft.unitId.empty()?
                            "All owned Units":draft.unitId;
                        if(ImGui::BeginCombo("Count Unit class",current.c_str())){
                            if(ImGui::Selectable("All owned Units",draft.unitId.empty()))
                                draft.unitId.clear();
                            for(const auto& choice:project.catalog.entries())
                                if(choice.type=="Unit" &&
                                   ImGui::Selectable(choice.id.c_str(),
                                     draft.unitId==choice.id))
                                    draft.unitId=choice.id;
                            ImGui::EndCombo();
                        }
                    }
                    if(draft.kind==RTSValidatorKind::HasBehavior){
                        const std::string current=draft.behaviorId.empty()?
                            "Choose a Behavior":draft.behaviorId;
                        if(ImGui::BeginCombo("Active Behavior",current.c_str())){
                            for(const auto& choice:project.catalog.entries())
                                if(choice.type=="Behavior" &&
                                   ImGui::Selectable(choice.id.c_str(),
                                     draft.behaviorId==choice.id))
                                    draft.behaviorId=choice.id;
                            ImGui::EndCombo();
                        }
                        if(!draft.behaviorId.empty() &&
                           navigateCatalog(app,"Edit Behavior",draft.behaviorId)){
                            endPane();return;
                        }
                    }
                    ImGui::TextWrapped("Caster tests the issuing Unit; Target tests the ability's selected Unit. Ground and training gates have no Target.");
                    if(ImGui::Button("Apply Validator")){
                        if(ed.setValidatorDefinition(project,draft)){
                            refresh(app,true);editingValidator.clear();
                            endPane();return;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Revert Validator"))
                        draft=compileValidator(project.catalog,entry->id);
                    ImGui::TextDisabled("Typed, deterministic runtime conditions");
                }
                if(entry->type=="Requirement"){
                    ImGui::SeparatorText("Requirement graph | Logic");
                    static std::string editingRequirement;
                    static int operation=0;
                    static std::string validatorId;
                    static std::vector<std::string> childIds;
                    if(editingRequirement!=entry->id){
                        const auto& properties=resolved;
                        const auto findString=[&](const char* field)->std::string{
                            auto it=properties.find(field);
                            return it!=properties.end() &&
                                it->second.template is<std::string>()?
                                it->second.template get<std::string>():"";
                        };
                        const auto mode=findString("kind");
                        operation=mode=="All"?1:mode=="Any"?2:mode=="Not"?3:0;
                        validatorId=findString("validator");
                        childIds.clear();
                        const auto it=properties.find("children");
                        if(it!=properties.end() &&
                           it->second.template is<picojson::array>())
                            for(const auto& child:it->second.template get<picojson::array>())
                                if(child.template is<std::string>())
                                    childIds.push_back(child.template get<std::string>());
                        editingRequirement=entry->id;
                    }
                    const char* modes[]={"Predicate","All","Any","Not"};
                    ImGui::Combo("Boolean operation",&operation,modes,4);
                    if(operation==0){
                        if(ImGui::BeginCombo("Predicate Validator",
                            validatorId.empty()?"Choose Validator":validatorId.c_str())){
                            for(const auto& choice:project.catalog.entries())
                                if(choice.type=="Validator" &&
                                   ImGui::Selectable(choice.id.c_str(),
                                                     validatorId==choice.id))
                                    validatorId=choice.id;
                            ImGui::EndCombo();
                        }
                        if(!validatorId.empty() &&
                           navigateCatalog(app,"Open Validator",validatorId)){
                            endPane();return;
                        }
                    }else{
                        ImGui::TextDisabled("Nested Requirement children (%zu / %d)",
                                           childIds.size(),operation==3?1:8);
                        for(std::size_t i=0;i<childIds.size();++i){
                            ImGui::PushID(static_cast<int>(i));
                            const std::string id=childIds[i];
                            if(navigateCatalog(app,"Child",id)){
                                ImGui::PopID();endPane();return;
                            }
                            ImGui::SameLine();
                            if(ImGui::SmallButton("Remove")){
                                childIds.erase(childIds.begin()+
                                    static_cast<std::ptrdiff_t>(i));
                                ImGui::PopID();break;
                            }
                            if(i>0){
                                ImGui::SameLine();
                                if(ImGui::SmallButton("Up"))
                                    std::swap(childIds[i],childIds[i-1]);
                            }
                            ImGui::PopID();
                        }
                        if(childIds.size()<std::size_t(operation==3?1:8) &&
                           ImGui::BeginCombo("Add graph child","Choose Requirement")){
                            for(const auto& candidate:project.catalog.entries())
                                if(candidate.type=="Requirement" &&
                                   candidate.id!=entry->id &&
                                   std::find(childIds.begin(),childIds.end(),
                                             candidate.id)==childIds.end() &&
                                   ImGui::Selectable(candidate.id.c_str()))
                                    childIds.push_back(candidate.id);
                            ImGui::EndCombo();
                        }
                        ImGui::TextWrapped("All: every child true. Any: one child true. Not: exactly one child, inverted. Cyclic/oversized graphs are rejected.");
                    }
                    if(ImGui::Button("Apply Requirement graph")){
                        if(ed.setRequirementDefinition(project,
                                static_cast<RTSRequirementKind>(operation),
                                validatorId,childIds)){
                            refresh(app,true);editingRequirement.clear();
                            endPane();return;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Discard graph draft"))
                        editingRequirement.clear();
                    ImGui::TextDisabled("8 levels | 8 children/node | 64 expanded nodes");
                }
                if(entry->type=="Weapon"){
                    ImGui::SeparatorText("Normal attack | Weapon catalog");
                    static WeaponDefinition draft;
                    static std::string editingWeapon;
                    if(editingWeapon!=entry->id){
                        draft=compileWeapon(project.catalog,entry->id);
                        editingWeapon=entry->id;
                    }
                    ImGui::InputFloat("Attack damage",&draft.damage,.5f,2.f,"%.2f");
                    ImGui::InputFloat("Attack reach",&draft.range,.25f,1.f,"%.2f");
                    ImGui::InputFloat("Cooldown (seconds)",&draft.cooldown,.05f,.25f,"%.2f");
                    ImGui::InputFloat("Armor penetration",&draft.armorPiercing,.5f,2.f,"%.2f");
                    if(ImGui::BeginCombo("Projectile delivery",
                         draft.projectileId.empty()?"Instant (legacy)":draft.projectileId.c_str())){
                        if(ImGui::Selectable("Instant (legacy)",draft.projectileId.empty()))
                            draft.projectileId.clear();
                        for(const auto& candidate:project.catalog.entries())
                            if(candidate.type=="Projectile" &&
                               ImGui::Selectable(candidate.id.c_str(),
                                                 draft.projectileId==candidate.id))
                                draft.projectileId=candidate.id;
                        ImGui::EndCombo();
                    }
                    if(!draft.projectileId.empty() &&
                       navigateCatalog(app,"Edit delivery Projectile",draft.projectileId)){
                        endPane();return;
                    }
                    ImGui::TextWrapped("Projectile attacks use fixed-tick swept contact; targets may dodge. Intercept units on flight is configured in the linked Projectile.");
                    if(ImGui::Button("Apply weapon settings")){
                        if(ed.setWeaponDefinition(project,draft)){
                            refresh(app,true);editingWeapon.clear();
                            endPane();return;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Revert weapon"))
                        draft=compileWeapon(project.catalog,entry->id);
                    ImGui::TextDisabled("Damage 0..10000 | Reach 0..128");
                    ImGui::TextDisabled("Cooldown .05..60s | Pierce 0..1000");
                    ImGui::TextWrapped("Equip this reusable Weapon on a Unit. "
                        "The profile controls ordinary attack damage, "
                        "range, cooldown and mitigation; Ability damage stays separate.");
                }
                if(entry->type=="Projectile"){
                    ImGui::SeparatorText("Projectile delivery | Tick-driven");
                    static ProjectileDefinition draft;
                    static std::string editingProjectile;
                    if(editingProjectile!=entry->id){
                        draft=compileProjectile(project.catalog,entry->id);
                        editingProjectile=entry->id;
                    }
                    ImGui::InputFloat("Speed (units/s)",&draft.speed,.5f,3.f,"%.2f");
                    ImGui::InputFloat("Arc height",&draft.arcHeight,.25f,1.f,"%.2f");
                    ImGui::InputFloat("Visual size",&draft.visualScale,.05f,.25f,"%.2f");
                    ImGui::InputFloat("Contact radius",&draft.collisionRadius,.05f,.25f,"%.2f");
                    ImGui::Checkbox("Intercept units during flight",&draft.collideUnits);
                    float tint[3]={draft.tint.x,draft.tint.y,draft.tint.z};
                    if(ImGui::ColorEdit3("Projectile color",tint))
                        draft.tint=Vec3(tint[0],tint[1],tint[2]);
                    if(ImGui::Button("Apply projectile settings")){
                        if(ed.setProjectileDefinition(project,draft)){
                            refresh(app,true);editingProjectile.clear();
                            endPane();return;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Revert projectile")){
                        draft=compileProjectile(project.catalog,entry->id);
                    }
                    ImGui::TextDisabled("Speed .5..100 | Arc 0..24 | Contact .05..4");
                    ImGui::TextWrapped("A Ground Ability can reference this projectile. Flight time is based on distance and speed; optional contact interrupts the flight.");
                }
                if(entry->type=="Behavior"){
                    ImGui::SeparatorText("Status behavior | Live modifiers");
                    static BehaviorDefinition draft;
                    static std::string draftObject;
                    if(draftObject!=entry->id){
                        draft=compileBehavior(project.catalog,entry->id);
                        draftObject=entry->id;
                    }
                    int duration=int(draft.durationTicks);
                    ImGui::InputInt("Duration (ticks)",&duration,15,60);
                    draft.durationTicks=static_cast<std::uint32_t>(std::max(0,duration));
                    ImGui::InputInt("Max stacks",&draft.maxStacks);
                    ImGui::InputFloat("Armor per stack",&draft.armorBonus,.5f,1.f,"%.2f");
                    ImGui::InputFloat("Damage per stack",&draft.damageBonus,1.f,5.f,"%.2f");
                    ImGui::InputFloat("Speed multiplier",&draft.speedMultiplier,.05f,.1f,"%.2f");
                    ImGui::Checkbox("Refresh duration on reapply",&draft.refreshDuration);
                    ImGui::Checkbox("Can be dispelled",&draft.dispellable);
                    if(ImGui::Button("Apply behavior modifiers")){
                        if(ed.setBehaviorDefinition(project,draft)){
                            refresh(app,true);
                            draftObject.clear();
                            endPane();return;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Revert pending changes")){
                        draft=compileBehavior(project.catalog,entry->id);
                    }
                    ImGui::TextDisabled("Duration 1..36000 | Stacks 1..8 | Modifier cap is validated");
                }
                if(entry->type=="Ability" || entry->type=="Unit"){
                    const auto it=resolved.find("requirement");
                    const std::string required=it!=resolved.end() &&
                        it->second.template is<std::string>()?
                        it->second.template get<std::string>():"";
                    ImGui::SeparatorText(entry->type=="Unit"?
                        "Training Requirement":"Ability Requirement");
                    if(ImGui::BeginCombo("Required graph",
                            required.empty()?"None (unrestricted)":required.c_str())){
                        if(ImGui::Selectable("None (unrestricted)",required.empty())){
                            refresh(app,ed.setCatalogField(project,"requirement",
                                picojson::value(std::string{})));
                            ImGui::EndCombo();endPane();return;
                        }
                        for(const auto& choice:project.catalog.entries())
                            if(choice.type=="Requirement" &&
                               ImGui::Selectable(choice.id.c_str(),
                                                 required==choice.id)){
                                refresh(app,ed.setCatalogField(project,"requirement",
                                    picojson::value(choice.id)));
                                ImGui::EndCombo();endPane();return;
                            }
                        ImGui::EndCombo();
                    }
                    if(!required.empty() &&
                       navigateCatalog(app,"Open required graph",required)){
                        endPane();return;
                    }
                    ImGui::TextDisabled(entry->type=="Unit"?
                        "Checked by production queue admission":
                        "Checked at actual cast target/ground admission");
                    ImGui::SeparatorText("Show versus Use requirements");
                    const auto showField=resolved.find("show_requirement");
                    const std::string showReq=showField!=resolved.end() &&
                        showField->second.template is<std::string>()?
                        showField->second.template get<std::string>():"";
                    if(ImGui::BeginCombo("Show command when",
                        showReq.empty()?"Always visible":showReq.c_str())){
                        if(ImGui::Selectable("Always visible",showReq.empty())){
                            refresh(app,ed.setCatalogField(project,
                                "show_requirement",picojson::value(std::string{})));
                            ImGui::EndCombo();endPane();return;
                        }
                        for(const auto& candidate:project.catalog.entries())
                            if(candidate.type=="Requirement" &&
                               ImGui::Selectable(candidate.id.c_str(),
                                                 candidate.id==showReq)){
                                refresh(app,ed.setCatalogField(project,
                                    "show_requirement",picojson::value(candidate.id)));
                                ImGui::EndCombo();endPane();return;
                            }
                        ImGui::EndCombo();
                    }
                    if(!showReq.empty() &&
                       navigateCatalog(app,"Open Show graph",showReq)){
                        endPane();return;
                    }
                    ImGui::TextWrapped("Show hides commands entirely until its Caster-only graph passes. Use leaves visible commands disabled until the live prerequisite passes.");
                }
                if(entry->type=="Ability"){
                    const auto effectRef=resolved.find("effect");
                    if(effectRef!=resolved.end() &&
                       effectRef->second.template is<std::string>() &&
                       navigateCatalog(app,"Effect",effectRef->second.template get<std::string>())){
                        endPane();return;
                    }
                    const auto target=resolved.find("target");
                    const std::string current=target!=resolved.end() &&
                       target->second.template is<std::string>()?
                       target->second.template get<std::string>():"Self";
                    if(ImGui::BeginCombo("Target policy",current.c_str())){
                        for(const char* kind:{"Self","Ally","Enemy","Any","Ground"}){
                            if(ImGui::Selectable(kind,kind==current) && kind!=current){
                                refresh(app,ed.setAbilityTargetPolicy(project,kind));
                                ImGui::EndCombo();endPane();return;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if(ImGui::BeginCombo("Applied effect","Choose Effect...")){
                        for(const auto& option:project.catalog.entries())
                            if(option.type=="Effect" &&
                               ImGui::Selectable(option.id.c_str())){
                                refresh(app,ed.setCatalogField(project,"effect",
                                    picojson::value(option.id)));
                                ImGui::EndCombo();endPane();return;
                            }
                        ImGui::EndCombo();
                    }
                    if(current=="Ground"){
                        const auto defined=compileAbility(project.catalog,entry->id);
                        const std::string projectileRef=defined.projectileId.empty()?
                            "None | impact scheduler":defined.projectileId;
                        ImGui::SeparatorText("Projectile delivery");
                        if(ImGui::BeginCombo("Projectile",projectileRef.c_str())){
                            if(ImGui::Selectable("None | impact scheduler",
                                                 defined.projectileId.empty())){
                                refresh(app,ed.setCatalogField(project,"projectile",
                                          picojson::value(std::string{})));
                                ImGui::EndCombo();endPane();return;
                            }
                            for(const auto& candidate:project.catalog.entries()){
                                if(candidate.type!="Projectile")continue;
                                if(ImGui::Selectable(candidate.id.c_str(),
                                     candidate.id==defined.projectileId)){
                                    refresh(app,ed.setCatalogField(project,
                                          "projectile",picojson::value(candidate.id)));
                                    ImGui::EndCombo();endPane();return;
                                }
                            }
                            ImGui::EndCombo();
                        }
                        if(!defined.projectileId.empty() &&
                           navigateCatalog(app,"Edit projectile",defined.projectileId)){
                            endPane();return;
                        }
                        ImGui::TextWrapped(defined.projectileId.empty()?
                            "No projectile: delay counts directly from cast.":
                            "Delivery takes distance / projectile speed, then adds the authored post-flight delay.");
                        static std::string editingAbility;
                        static float radius=4.f;
                        static int delay=60;
                        static int team=0;
                        static int shape=0;
                        static float angle=90.f,width=2.f;
                        const char* shapes[]={"Circle","Cone","Line"};
                        if(editingAbility!=entry->id){
                            const auto ground=compileAbility(project.catalog,entry->id);
                            radius=ground.areaRadius;delay=int(ground.impactDelayTicks);
                            shape=ground.areaShape==RTSAreaShape::Cone?1:
                                  ground.areaShape==RTSAreaShape::Line?2:0;
                            angle=ground.areaAngle;width=ground.areaWidth;
                            team=ground.areaTeam==AbilityAreaTeam::Enemy?0:
                                 ground.areaTeam==AbilityAreaTeam::Ally?1:2;
                            editingAbility=entry->id;
                        }
                        ImGui::SeparatorText("Ground area & impact");
                        ImGui::Combo("Search shape",&shape,shapes,3);
                        ImGui::InputFloat("Area radius",&radius,.25f,1.f,"%.2f");
                        if(shape==1)ImGui::InputFloat("Cone angle (degrees)",&angle,5.f,15.f,"%.1f");
                        if(shape==2)ImGui::InputFloat("Line width",&width,.25f,1.f,"%.2f");
                        ImGui::InputInt("Impact delay (ticks)",&delay,15,60);
                        const char* teams[]={"Enemy","Ally","Any"};
                        ImGui::Combo("Affected team",&team,teams,3);
                        if(ImGui::Button("Apply ground settings")){
                            if(ed.setGroundAbilitySettings(project,radius,
                                    teams[std::clamp(team,0,2)],delay,
                                    shapes[std::clamp(shape,0,2)],angle,width)){
                                refresh(app,true);
                                editingAbility.clear();
                                endPane();return;
                            }
                        }
                        ImGui::SameLine();
                        if(ImGui::SmallButton("Revert area changes")){
                            const auto defined=compileAbility(project.catalog,entry->id);
                            radius=defined.areaRadius;
                            shape=defined.areaShape==RTSAreaShape::Cone?1:
                                  defined.areaShape==RTSAreaShape::Line?2:0;
                            angle=defined.areaAngle;width=defined.areaWidth;
                            delay=int(defined.impactDelayTicks);
                            team=defined.areaTeam==AbilityAreaTeam::Enemy?0:
                                 defined.areaTeam==AbilityAreaTeam::Ally?1:2;
                        }
                        ImGui::TextDisabled("Radius .5..24 | Cone 10..170 deg | Line .25..12 wide");
                        ImGui::TextDisabled("Impact 0..300 ticks | Max 64 Units");
                        CreatorAreaShapePreview::draw(
                            shape==1?RTSAreaShape::Cone:
                                shape==2?RTSAreaShape::Line:RTSAreaShape::Circle,
                            radius,angle,width);
                        ImGui::TextWrapped("Cones and lines extend from the impact point away from the caster. Direction is fixed at cast time; occupants resolve on impact.");
                        ImGui::TextWrapped("The ground point must be visible. Delayed impacts hit units at arrival, not when cast.");
                    }
                    ImGui::TextDisabled("Cooldown: ticks at 60Hz | Cast range: world units");
                }
                if(entry->type=="Unit"){
                    ImGui::SeparatorText("Basic attack weapon");
                    const auto linked=resolved.find("weapon");
                    const std::string weaponId=linked!=resolved.end() &&
                        linked->second.template is<std::string>()?
                        linked->second.template get<std::string>():"";
                    if(ImGui::BeginCombo("Equipped weapon",
                                         weaponId.empty()?"Legacy unit stats":weaponId.c_str())){
                        if(ImGui::Selectable("Legacy unit stats",weaponId.empty())){
                            if(ed.setCatalogField(project,"weapon",
                                                   picojson::value(std::string{}))){
                                refresh(app,true);ImGui::EndCombo();endPane();return;
                            }
                        }
                        for(const auto& item:project.catalog.entries()){
                            if(item.type!="Weapon")continue;
                            if(ImGui::Selectable(item.id.c_str(),weaponId==item.id)){
                                if(ed.setCatalogField(project,"weapon",
                                                       picojson::value(item.id))){
                                    refresh(app,true);ImGui::EndCombo();endPane();return;
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if(!weaponId.empty() &&
                       navigateCatalog(app,"Edit equipped weapon",weaponId)){
                        endPane();return;
                    }
                    const auto resolvedAttack=unitFromCatalog(project.catalog,entry->id);
                    ImGui::TextDisabled("Damage %.2f | Reach %.2f | Cooldown %.2fs | Pierce %.2f",
                        resolvedAttack.damage,resolvedAttack.attack_range,
                        resolvedAttack.attack_cooldown,resolvedAttack.armor_piercing);
                    ImGui::SeparatorText("Ability loadout");
                    const auto abilities=resolved.find("abilities");
                    if(abilities==resolved.end()){
                        if(ImGui::SmallButton("Enable ability slots")){
                            refresh(app,ed.setCatalogField(project,"abilities",
                                picojson::value(picojson::array{})));
                            endPane();return;
                        }
                    }else if(CreatorReferenceWidgets::draw(
                        app,entry->id,"abilities",abilities->second)){
                        endPane();return;
                    }
                    auto researchable=resolved.find("researchable_upgrades");
                    if(researchable==resolved.end()){
                        if(ImGui::SmallButton("Enable research options")){
                            refresh(app,ed.setCatalogField(project,
                                "researchable_upgrades",
                                picojson::value(picojson::array{})));
                            endPane();return;
                        }
                    }else if(CreatorReferenceWidgets::draw(
                        app,entry->id,"researchable_upgrades",researchable->second)){
                        endPane();return;
                    }
                    auto training=resolved.find("trainable_units");
                    if(training!=resolved.end() && training->second.template is<picojson::array>()){
                        for(const auto& value:training->second.template get<picojson::array>()){
                            if(value.template is<std::string>() &&
                               navigateCatalog(app,"Trains",value.template get<std::string>())){
                                endPane();return;
                            }
                        }
                    }
                }
                if(entry->type=="Effect"){
                    const auto kindIt=resolved.find("kind");
                    const std::string currentKind=kindIt!=resolved.end() &&
                        kindIt->second.template is<std::string>()?
                        kindIt->second.template get<std::string>():"(unset)";
                    if(ImGui::BeginCombo("Effect operation",currentKind.c_str())){
                        for(const char* kind:{"Damage","Heal","PeriodicDamage",
                                             "PeriodicHeal","ApplyBehavior",
                                             "RemoveBehavior","Sequence"}){
                            if(ImGui::Selectable(kind,kind==currentKind) &&
                               kind!=currentKind){
                                refresh(app,ed.convertEffectKind(project,kind));
                                ImGui::EndCombo();endPane();return;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if(currentKind=="ApplyBehavior" || currentKind=="RemoveBehavior"){
                        const auto linked=resolved.find("behavior");
                        const std::string selected=linked!=resolved.end() &&
                            linked->second.template is<std::string>()?
                            linked->second.template get<std::string>():"(unlinked)";
                        if(navigateCatalog(app,"Status Behavior",selected)){
                            endPane();return;
                        }
                        if(ImGui::BeginCombo("Status reference",selected.c_str())){
                            for(const auto& candidate:project.catalog.entries())
                                if(candidate.type=="Behavior" &&
                                   ImGui::Selectable(candidate.id.c_str(),
                                                      candidate.id==selected)){
                                    refresh(app,ed.setCatalogField(project,"behavior",
                                        picojson::value(candidate.id)));
                                    ImGui::EndCombo();endPane();return;
                                }
                            ImGui::EndCombo();
                        }
                        ImGui::TextWrapped("%s reuses a typed Behavior; "
                                          "apply stacks or dispel according to its rules.",
                                          currentKind.c_str());
                    }
                    if(currentKind=="PeriodicDamage" || currentKind=="PeriodicHeal"){
                        ImGui::TextDisabled("Ticks at 60 Hz | interval 1..600, duration 1..3600");
                        ImGui::TextWrapped("Use Resolved properties below to edit pulse size, interval_ticks and duration_ticks.");
                    }
                }
                if(entry->type=="Effect"){
                    ImGui::SeparatorText("Visual effect flow");
                    const std::string clicked=CreatorEffectGraphWidgets::draw(
                        project.catalog,entry->id);
                    if(!clicked.empty() && clicked!=entry->id &&
                       ed.selectCatalogId(project,clicked)){
                        endPane();return;
                    }
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
                                    ImGui::PopID();endPane();return;
                                }
                                if(i>0){
                                    ImGui::SameLine();
                                    if(ImGui::SmallButton("Up")){
                                        auto changed=children;
                                        std::swap(changed[i],changed[i-1]);
                                        refresh(app,ed.setCatalogField(project,"children",
                                            picojson::value(changed)));
                                        ImGui::PopID();endPane();return;
                                    }
                                }
                                if(i+1<children.size()){
                                    ImGui::SameLine();
                                    if(ImGui::SmallButton("Down")){
                                        auto changed=children;
                                        std::swap(changed[i],changed[i+1]);
                                        refresh(app,ed.setCatalogField(project,"children",
                                            picojson::value(changed)));
                                        ImGui::PopID();endPane();return;
                                    }
                                }
                                if(children.size()>1){
                                    ImGui::SameLine();
                                    if(ImGui::SmallButton("Remove")){
                                        auto changed=children;
                                        changed.erase(changed.begin()+static_cast<std::ptrdiff_t>(i));
                                        refresh(app,ed.setCatalogField(project,"children",
                                            picojson::value(changed)));
                                        ImGui::PopID();endPane();return;
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
                                            ImGui::EndCombo();endPane();return;
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
                                              name+" = "+rtsFormatCatalogPreview(resolved.at(name));
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
                        }else if(entry->type=="Unit" && field=="abilities"){
                            ImGui::TextDisabled("Use the Ability loadout editor above");
                        }else if(found->second.template is<picojson::array>() &&
                            ((entry->type=="Unit" && field=="trainable_units") ||
                             (entry->type=="Effect" && field=="children"))){
                            if(CreatorReferenceWidgets::draw(app,entry->id,field,found->second)){
                                endPane();return;
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
                endPane();return;
            }
            ImGui::Text("Owner: %d",copy.owner);
            if(ImGui::Button("Switch owner")){
                refresh(app,ed.toggleSelectedOwner(project));
                endPane();return;
            }
            ImGui::SameLine();
            if(ImGui::Button("Duplicate")){
                refresh(app,ed.duplicateSelectedPlacement(project));
                endPane();return;
            }
            ImGui::SameLine();
            if(ImGui::Button("Delete")){
                refresh(app,ed.deleteSelectedPlacement(project));
                endPane();return;
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
    endPane();
}
template<class App>
void moduleWorkspace(App& app){
    auto& ed=app.editor;
    const auto* viewport=ImGui::GetMainViewport();
    const float scale=ImGui::GetFontSize()/16.5f;
    // Unique window IDs preserve the geometry for each editor module.
    const std::string title=std::string(creatorModuleTitle(
        static_cast<int>(ed.panel)))+"###creator_module_"+
        std::to_string(static_cast<int>(ed.panel));
    const ImVec2 area=viewport->Size;
    ImGui::SetNextWindowPos(
        ImVec2(viewport->Pos.x+app.moduleStartOffset.x,
               viewport->Pos.y+app.moduleStartOffset.y),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(std::min(area.x*.89f,std::max(710.f*scale,area.x*.73f)),
               std::max(355.f*scale,area.y-100.f*scale)),
        ImGuiCond_FirstUseEver);
    // Pinned ImGui docking exposes per-window NoAutoMerge: even while
    // overlapping the game's window this creates a REAL native OS window,
    // with its own decoration/taskbar entry, not merely a detached panel.
    if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable){
        ImGuiWindowClass nativeClass;
        nativeClass.ViewportFlagsOverrideSet=ImGuiViewportFlags_NoAutoMerge;
        nativeClass.ViewportFlagsOverrideClear=
            ImGuiViewportFlags_NoDecoration|ImGuiViewportFlags_NoTaskBarIcon;
        ImGui::SetNextWindowClass(&nativeClass);
    }
    bool open=true;
    const bool body=ImGui::Begin(title.c_str(),&open,
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDocking);
    if(body){
        ImGui::TextColored(ImVec4(.48f,.83f,.96f,1.f),"CREATOR  /  %s",
            creatorModuleTitle(static_cast<int>(ed.panel)));
        ImGui::SameLine();
        if(ImGui::SmallButton("Save"))app.saveProject();
        ImGui::SameLine();
        if(ImGui::SmallButton("Undo"))refresh(app,ed.undo(app.project));
        ImGui::SameLine();
        if(ImGui::SmallButton("Redo"))refresh(app,ed.redo(app.project));
        ImGui::SameLine();
        if(ImGui::SmallButton("Back to Playtest"))
            choosePanel(app,EditorSession::Panel::Play);
        ImGui::SetNextItemWidth(240.f*scale);
        if(ImGui::BeginCombo("Switch module",
              creatorModuleTitle(static_cast<int>(ed.panel)))){
            const EditorSession::Panel choices[]={
                EditorSession::Panel::Terrain,EditorSession::Panel::Triggers,
                EditorSession::Panel::Data,EditorSession::Panel::Scene};
            for(const auto next:choices)
                if(ImGui::Selectable(creatorModuleTitle(static_cast<int>(next)),
                                     ed.panel==next))
                    choosePanel(app,next);
            ImGui::EndCombo();
        }
        ImGui::Separator();
        const ImGuiTableFlags flags=ImGuiTableFlags_Resizable|
            ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_SizingStretchProp;
        if(ed.panel!=EditorSession::Panel::Play &&
           ImGui::BeginTable("##creatorModuleGrid",2,flags,
                             ImVec2(0,ImGui::GetContentRegionAvail().y))){
            ImGui::TableSetupColumn("Navigator",
                ImGuiTableColumnFlags_WidthFixed,315.f*scale);
            ImGui::TableSetupColumn("Properties",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            modulePane=true;
            ImGui::TableSetColumnIndex(0);
            browser(app);
            ImGui::TableSetColumnIndex(1);
            inspector(app);
            modulePane=false;
            ImGui::EndTable();
        }
    }
    ImGui::End();
    if(!open)choosePanel(app,EditorSession::Panel::Play);
}
template<class App>
void draw(App& app){
    if(app.pauseMenu)return;
    toolbar(app);
    CreatorTriggerMonitor::draw(app);
    CreatorAIResearch::draw(app);
    if(app.editor.panel==EditorSession::Panel::Play)return;
    if(app.uiSettings.separateModules){
        // Drag the title bar outside the game viewport for an independent
        // native GLFW/OS window; the project/controller stays shared.
        moduleWorkspace(app);
    }else{
        ImGui::DockSpaceOverViewport(0,ImGui::GetMainViewport(),
                                     ImGuiDockNodeFlags_PassthruCentralNode);
        browser(app);
        inspector(app);
    }
    CreatorCatalogNavigator::draw(app);
    CreatorTechGraph::draw(app);
}
} // namespace CreatorWorkbench
