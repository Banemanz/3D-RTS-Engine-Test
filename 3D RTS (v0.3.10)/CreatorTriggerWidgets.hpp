#pragma once
#include "EditorSession.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

// Galaxy-like event / condition / action tree. UI is a replaceable adapter:
// every mutation goes through the same validated, undoable headless API.
namespace CreatorTriggerWidgets {
template<class App>
void refresh(App& app,bool changed) {
    if(!changed)return;
    try{app.reloadFromProject();}
    catch(const std::exception& e){
        app.editor.message=std::string("TRIGGER RELOAD FAILED: ")+e.what();
    }
}
template<class App>
void field(App& app,const std::string& ruleId,bool event,bool condition,
           std::size_t nodeIndex,const std::string& name,const picojson::value& value){
    auto& ed=app.editor;
    auto& project=app.project;
    const std::string identity=ruleId+(event?".event.":condition?".condition.":".action.")+
                               std::to_string(nodeIndex)+"."+name;
    ImGui::PushID(identity.c_str());
    if(name=="catalog" || name=="region" || name=="result" || name=="effect"){
        if(value.template is<std::string>()){
            const std::string current=value.template get<std::string>();
            std::vector<std::string> options;
            if(name=="result")options={"Victory","Defeat"};
            else if(name=="region"){
                for(const auto& placement:project.placements)
                    if(placement.kind=="Region")options.push_back(placement.id);
            }else{
                for(const auto& entry:project.catalog.entries())
                    if(entry.type==(name=="effect"?"Effect":"Unit"))
                        options.push_back(entry.id);
            }
            std::string chosen;
            if(ImGui::BeginCombo(name.c_str(),current.c_str())){
                for(const auto& option:options)
                    if(ImGui::Selectable(option.c_str(),current==option))
                        chosen=option;
                ImGui::EndCombo();
            }
            if(!chosen.empty() && chosen!=current){
                const picojson::value updated(chosen);
                if(event)refresh(app,ed.setTriggerEventField(project,name,updated));
                else refresh(app,ed.setTriggerNodeField(project,condition,nodeIndex,name,updated));
            }
        }
    }else if(value.template is<bool>()){
        bool current=value.template get<bool>();
        if(ImGui::Checkbox(name.c_str(),&current)){
            if(event)refresh(app,ed.setTriggerEventField(project,name,picojson::value(current)));
            else refresh(app,ed.setTriggerNodeField(project,condition,nodeIndex,name,
                                                   picojson::value(current)));
        }
    }else if(value.template is<double>()){
        static std::unordered_map<std::string,double> draft;
        const double current=value.template get<double>();
        auto inserted=draft.emplace(identity,current);
        double& number=inserted.first->second;
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputDouble(name.c_str(),&number,0.,0.,"%.3f");
        const bool committed=ImGui::IsItemDeactivatedAfterEdit();
        const bool active=ImGui::IsItemActive();
        if(committed){
            if(event)refresh(app,ed.setTriggerEventField(project,name,picojson::value(number)));
            else refresh(app,ed.setTriggerNodeField(project,condition,nodeIndex,name,
                                                   picojson::value(number)));
            // A rejected edit restores the authoritative document next frame.
            number=current;
        }else if(!active)number=current;
    }else if(value.template is<std::string>()){
        static std::unordered_map<std::string,std::string> draft;
        const auto& current=value.template get<std::string>();
        auto inserted=draft.emplace(identity,current);
        auto& text=inserted.first->second;
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText(name.c_str(),&text);
        const bool committed=ImGui::IsItemDeactivatedAfterEdit();
        const bool active=ImGui::IsItemActive();
        if(committed){
            if(event)refresh(app,ed.setTriggerEventField(project,name,picojson::value(text)));
            else refresh(app,ed.setTriggerNodeField(project,condition,nodeIndex,name,
                                                   picojson::value(text)));
            text=current;
        }else if(!active)text=current;
    }else ImGui::TextDisabled("%s: edit in advanced JSON",name.c_str());
    ImGui::PopID();
}
template<class App>
void nodes(App& app,const picojson::object& rule,const std::string& id,bool condition){
    auto& ed=app.editor;auto& project=app.project;
    const char* bucket=condition?"conditions":"actions";
    auto it=rule.find(bucket);
    const picojson::array empty;
    const auto& values=(it!=rule.end() && it->second.template is<picojson::array>())
        ?it->second.template get<picojson::array>():empty;
    ImGui::SeparatorText(condition?"Conditions (all must pass)":"Actions (in order)");
    for(std::size_t index=0;index<values.size();++index){
        if(!values[index].template is<picojson::object>())continue;
        const auto node=values[index].template get<picojson::object>();
        const auto t=node.find("type");
        const std::string kind=t!=node.end() && t->second.template is<std::string>()
            ?t->second.template get<std::string>():"<UNKNOWN>";
        ImGui::PushID(int(index)+(condition?1000:2000));
        const std::string label=std::to_string(index+1)+". "+kind;
        if(ImGui::TreeNodeEx(label.c_str(),ImGuiTreeNodeFlags_DefaultOpen)){
            bool moved=false;
            if(index>0 && ImGui::SmallButton("Up"))
                moved=ed.moveTriggerNode(project,condition,index,-1);
            if(index>0)ImGui::SameLine();
            if(index+1<values.size() && ImGui::SmallButton("Down"))
                moved=ed.moveTriggerNode(project,condition,index,1);
            if(index+1<values.size())ImGui::SameLine();
            if(ImGui::SmallButton("Remove"))
                moved=ed.removeTriggerNode(project,condition,index);
            if(moved)refresh(app,true);
            // Operate on this copied rule, not a pointer into a reloaded project.
            for(const auto& pair:node){
                if(pair.first=="type")continue;
                field(app,id,false,condition,index,pair.first,pair.second);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    const std::vector<std::string> options=condition
        ?std::vector<std::string>{"PlayerMineralsAtLeast","UnitCountAtLeast",
                                  "UnitTypeCountAtLeast","SubjectOwnerIs"}
        :std::vector<std::string>{"ShowMessage","GrantMinerals","SpawnUnit",
                                  "ApplyEffect","EndMatch"};
    std::string eventKind;
    auto event=rule.find("event");
    if(event!=rule.end() && event->second.template is<picojson::object>()){
        const auto& ev=event->second.template get<picojson::object>();
        auto it=ev.find("type");
        if(it!=ev.end() && it->second.template is<std::string>())
            eventKind=it->second.template get<std::string>();
    }
    if(ImGui::BeginCombo(condition?"Add condition...":"Add action...","Choose node type")){
        std::string chosen;
        for(const auto& type:options){
            const bool effectUnavailable=!condition && type=="ApplyEffect" &&
                                          eventKind!="OnRegionEnter";
            if(effectUnavailable)ImGui::BeginDisabled();
            if(ImGui::Selectable(type.c_str()) && !effectUnavailable)
                chosen=type;
            if(effectUnavailable){
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Requires a region-entering unit. Set event to OnRegionEnter first.");
                ImGui::EndDisabled();
            }
        }
        ImGui::EndCombo();
        if(!chosen.empty())refresh(app,ed.addTriggerNode(project,condition,chosen));
    }
}
template<class App>
void draw(App& app){
    auto& ed=app.editor;
    auto& project=app.project;
    const auto count=ed.triggerCount(project);
    ImGui::Text("%zu map rules",count);
    if(!count){ImGui::TextDisabled("Use New trigger in the Browser to begin.");return;}
    const auto rule=ed.triggerObject(project,ed.selectedTrigger%count);
    const auto idIt=rule.find("id");
    const std::string id=idIt!=rule.end() && idIt->second.template is<std::string>()
        ?idIt->second.template get<std::string>():"<unnamed>";
    ImGui::TextWrapped("Rule: %s",id.c_str());
    ImGui::TextDisabled("Changes validate before saving | Ctrl+Z / Ctrl+Y to undo and redo");
    if(ed.textEditing){
        ImGui::TextDisabled("Finish or cancel the JSON edit to use visual controls.");
        return;
    }
    const auto eventIt=rule.find("event");
    if(eventIt!=rule.end() && eventIt->second.template is<picojson::object>()){
        const auto event=eventIt->second.template get<picojson::object>();
        const auto typeIt=event.find("type");
        const std::string active=typeIt!=event.end() && typeIt->second.template is<std::string>()
            ?typeIt->second.template get<std::string>():"";
        ImGui::SeparatorText("Event");
        if(ImGui::BeginCombo("Event kind",active.c_str())){
            std::string next;
            for(const char* type:{"OnMapStart","After","Every","OnUnitDeath",
                                  "OnRegionEnter","MineralsBelow"}){
                if(ImGui::Selectable(type,active==type))next=type;
            }
            ImGui::EndCombo();
            if(!next.empty() && next!=active)
                refresh(app,ed.setTriggerEvent(project,next));
        }
        for(const auto& pair:event){
            if(pair.first=="type")continue;
            field(app,id,true,false,0,pair.first,pair.second);
        }
        auto onceIt=rule.find("once");
        bool once=onceIt!=rule.end() && onceIt->second.template is<bool>()
            ?onceIt->second.template get<bool>():false;
        if(ImGui::Checkbox("Run only once",&once))
            refresh(app,ed.setTriggerOnce(project,once));
    }
    ImGui::BeginChild("##ecaNodeTree",ImVec2(0,300.f),true);
    nodes(app,rule,id,true);
    nodes(app,rule,id,false);
    ImGui::EndChild();
    if(ImGui::CollapsingHeader("Advanced JSON / custom fields")){
        if(ImGui::Button("Edit full rule JSON"))ed.beginTriggerEdit(project);
        ImGui::TextWrapped("%s",ed.triggerJSON(project,ed.selectedTrigger%count).c_str());
    }
}
} // namespace CreatorTriggerWidgets
