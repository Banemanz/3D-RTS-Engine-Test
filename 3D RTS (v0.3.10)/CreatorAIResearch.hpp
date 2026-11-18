#pragma once
#include "RTSAIResearchPolicy.hpp"
#include "RTSUpgrades.hpp"
#include "EditorSession.hpp"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <vector>

// Cross-OS native creator panel. Gameplay executes only through the fixed
// command stream; this panel edits map metadata using EditorSession undo.
namespace CreatorAIResearch {
inline bool visible=false;
template<class App>
void draw(App& app){
    if(!visible || (app.editor.panel!=EditorSession::Panel::Data &&
                    app.editor.panel!=EditorSession::Panel::Play))return;
    const auto view=ImGui::GetIO().DisplaySize;
    const bool play=app.editor.panel==EditorSession::Panel::Play;
    // Reserve space for the browser on the left and the inspector on
    // the right in a 1280-wide editor; docking/manual resizing still win.
    constexpr float left=326.f,rightInspector=360.f,gutter=40.f;
    const float panelWidth=std::min(560.f,std::max(315.f,
        view.x-left-rightInspector-gutter));
    ImGui::SetNextWindowPos(ImVec2(left,65.f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panelWidth,
                                   std::min(play?225.f:570.f,view.y-95.f)),
                             ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Creator | Opponent Research",&visible)){
        ImGui::End();return;
    }
    auto settings=rtsAIResearchSettings(app.project);
    ImGui::TextDisabled("%s",play?"PLAYTEST | live opponent technology":
        "AUTHORING | fixed-step enemy research policy");
    if(!play)ImGui::TextWrapped("This is an opt-in opponent economy. Research uses actual producers, Show/Use Requirements, per-level costs and owner-specific stat upgrades. Attack AI remains independent.");
    if(app.editor.panel==EditorSession::Panel::Play){
        if(!settings.enabled){
            ImGui::SeparatorText("AI research disabled in this project");
            ImGui::TextWrapped("Use the F7 Data Editor to enable and configure opponent technology.");
        }else{
            ImGui::Text("Opponent research budget: %d",
                        app.game.enemyResearchMinerals());
            const auto jobs=app.game.researchWatch();
            for(const auto& id:settings.upgradePriorities){
                const auto def=compileUpgrade(app.project.catalog,id);
                const int level=app.game.upgradeLevel(1,id);
                ImGui::Text("%s  %d / %d",id.c_str(),level,def.maxLevel);
                for(const auto& job:jobs)if(job.ownerId==1 && job.upgradeId==id){
                    ImGui::ProgressBar(job.progress,ImVec2(-1.f,0.f));
                    ImGui::TextDisabled("Structure %d | Level %d | %llu ticks left",
                        job.producerId,job.nextLevel,
                        static_cast<unsigned long long>(job.remainingTicks));
                }
            }
        }
        ImGui::End();return;
    }
    const auto set=[&](const char* key,const picojson::value& v){
        if(!app.editor.setAIResearchSetting(app.project,key,v))return false;
        try{app.reloadFromProject();}
        catch(const std::exception& e){
            app.editor.message=std::string("AI RELOAD FAILED: ")+e.what();
        }
        return true;
    };
    bool enabled=settings.enabled;
    if(ImGui::Checkbox("Enable opponent research",&enabled))
        set("enemy_ai_research_enabled",picojson::value(enabled));
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Off by default. Global enemy_ai_enabled also gates opponent research.");
    int budget=settings.startingMinerals;
    if(ImGui::SliderInt("Starting mineral budget",&budget,0,500))
        set("enemy_ai_research_starting_minerals",picojson::value(double(budget)));
    int cadence=settings.intervalTicks;
    if(ImGui::SliderInt("Decision interval (ticks)",&cadence,2,300))
        set("enemy_ai_research_interval_ticks",picojson::value(double(cadence)));
    ImGui::TextDisabled("Budget up to 1,000,000 and interval up to 3,600 are");
    ImGui::TextDisabled("also accepted via project JSON for advanced scenarios.");
    ImGui::SeparatorText("Technology priority | highest first");
    ImGui::TextWrapped("AI skips unavailable or locked options and retries on its next decision tick. Technology research is never granted directly.");
    if(settings.upgradePriorities.empty())
        ImGui::TextDisabled("No technology selected. AI will not research.");
    for(std::size_t i=0;i<settings.upgradePriorities.size();++i){
        const std::string& id=settings.upgradePriorities[i];
        const auto def=compileUpgrade(app.project.catalog,id);
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("%zu. %s",i+1,id.c_str());
        ImGui::SameLine();
        if(ImGui::SmallButton("Inspect"))
            app.editor.selectCatalogId(app.project,id);
        ImGui::SameLine();
        if(ImGui::SmallButton("Up") && i>0){
            auto list=settings.upgradePriorities;
            std::swap(list[i],list[i-1]);
            picojson::array entries;
            for(const auto& item:list)entries.emplace_back(item);
            set("enemy_ai_upgrade_priorities",picojson::value(entries));
            ImGui::PopID();break;
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("Down") && i+1<settings.upgradePriorities.size()){
            auto list=settings.upgradePriorities;
            std::swap(list[i],list[i+1]);
            picojson::array entries;
            for(const auto& item:list)entries.emplace_back(item);
            set("enemy_ai_upgrade_priorities",picojson::value(entries));
            ImGui::PopID();break;
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("Remove")){
            picojson::array entries;
            for(const auto& existing:settings.upgradePriorities)
                if(existing!=id)entries.emplace_back(existing);
            set("enemy_ai_upgrade_priorities",picojson::value(entries));
            ImGui::PopID();break;
        }
        ImGui::TextDisabled("    %d levels | next cost %d | %u ticks",
            def.maxLevel,def.levels[0].mineralCost,def.levels[0].researchTicks);
        ImGui::PopID();
    }
    if(settings.upgradePriorities.size()<12 &&
       ImGui::BeginCombo("Add technology","Select an Upgrade...")){
        for(const auto& entry:app.project.catalog.entries()){
            if(entry.type!="Upgrade" ||
               std::find(settings.upgradePriorities.begin(),
                         settings.upgradePriorities.end(),entry.id)!=
                         settings.upgradePriorities.end())continue;
            if(ImGui::Selectable(entry.id.c_str())){
                picojson::array next;
                for(const auto& id:settings.upgradePriorities)
                    next.emplace_back(id);
                next.emplace_back(entry.id);
                set("enemy_ai_upgrade_priorities",picojson::value(next));
            }
        }
        ImGui::EndCombo();
    }
    bool producerFound=false;
    for(const auto& placed:app.project.placements)if(placed.kind=="Unit" &&
        placed.owner==1 && app.project.catalog.find(placed.catalogId)){
        const auto ids=unitResearchIds(app.project.catalog,placed.catalogId);
        for(const auto& id:settings.upgradePriorities)
            if(std::find(ids.begin(),ids.end(),id)!=ids.end())
                producerFound=true;
    }
    ImGui::SeparatorText("Scenario wiring");
    if(!settings.upgradePriorities.empty() && !producerFound)
        ImGui::TextColored(ImVec4(1.f,.70f,.28f,1.f),
            "No placed opponent Unit offers any prioritized research.");
    else if(producerFound)
        ImGui::TextColored(ImVec4(.54f,.87f,.70f,1.f),
            "Placed opponent research producer found.");
    ImGui::TextWrapped("To enable a producer, set the opposing structure's Unit catalog field researchable_upgrades to include these IDs. The normal F7 object inspector remains the source of truth.");
    if(!app.editor.message.empty()) {
        ImGui::SeparatorText("Editor status");
        ImGui::TextWrapped("%s",app.editor.message.c_str());
    }
    ImGui::End();
}
}
