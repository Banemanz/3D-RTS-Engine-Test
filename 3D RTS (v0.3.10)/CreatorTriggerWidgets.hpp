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
    if(name=="catalog" || name=="region" || name=="result" || name=="effect" ||
       name=="variable" || name=="operator"){
        if(value.template is<std::string>()){
            const std::string current=value.template get<std::string>();
            std::vector<std::string> options;
            if(name=="result")options={"Victory","Defeat"};
            else if(name=="operator")options={"==","!=","<","<=",">",">="};
            else if(name=="variable"){
                const auto it=project.extensions.find("trigger_variables");
                if(it!=project.extensions.end() && it->second.template is<picojson::object>())
                    for(const auto& entry:it->second.template get<picojson::object>())
                        options.push_back(entry.first);
            }else if(name=="region"){
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
            const bool canEvaluate=(condition && kind=="VariableCompare") ||
                (!condition && (kind=="SetVariable" || kind=="AddVariable" ||
                               kind=="GrantMinerals"));
            if(canEvaluate){
                const char* dynamicKey=kind=="GrantMinerals"?"amount_expr":"value_expr";
                auto exprIt=node.find(dynamicKey);
                const bool dynamic=exprIt!=node.end();
                const auto globalIt=project.extensions.find("trigger_variables");
                const bool hasGlobals=globalIt!=project.extensions.end() &&
                    globalIt->second.template is<picojson::object>() &&
                    !globalIt->second.template get<picojson::object>().empty();
                if(!dynamic && hasGlobals && ImGui::SmallButton("Use variable expression"))
                    refresh(app,ed.setTriggerNodeOperand(project,condition,index,true));
                if(dynamic){
                    if(ImGui::SmallButton("Use literal instead"))
                        refresh(app,ed.setTriggerNodeOperand(project,condition,index,false));
                    ImGui::TextDisabled("Advanced: formulas can be edited in full-rule JSON");
                    if(exprIt->second.template is<picojson::object>()){
                        const auto& expr=exprIt->second.template get<picojson::object>();
                        const auto op=expr.find("op");
                        const auto ref=expr.find("name");
                        if(op!=expr.end() && op->second.template is<std::string>() &&
                           op->second.template get<std::string>()=="Variable" &&
                           ref!=expr.end() && ref->second.template is<std::string>() &&
                           hasGlobals){
                            const auto current=ref->second.template get<std::string>();
                            if(ImGui::BeginCombo("Operand variable",current.c_str())){
                                for(const auto& value:
                                    globalIt->second.template get<picojson::object>()){
                                    if(ImGui::Selectable(value.first.c_str(),
                                                         current==value.first))
                                        refresh(app,ed.setTriggerNodeOperand(
                                            project,condition,index,true,value.first));
                                }
                                ImGui::EndCombo();
                            }
                        }
                    }
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    const std::vector<std::string> options=condition
        ?std::vector<std::string>{"PlayerMineralsAtLeast","UnitCountAtLeast",
                                  "UnitTypeCountAtLeast","SubjectOwnerIs","VariableCompare"}
        :std::vector<std::string>{"ShowMessage","GrantMinerals","SpawnUnit",
                                  "ApplyEffect","EndMatch","SetVariable","AddVariable"};
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
            const bool needsVariable=type=="VariableCompare" ||
                                     type=="SetVariable" || type=="AddVariable";
            const auto globals=project.extensions.find("trigger_variables");
            const bool variableUnavailable=needsVariable &&
                (globals==project.extensions.end() ||
                 !globals->second.template is<picojson::object>() ||
                 globals->second.template get<picojson::object>().empty());
            const bool disabled=effectUnavailable || variableUnavailable;
            if(disabled)ImGui::BeginDisabled();
            if(ImGui::Selectable(type.c_str()) && !disabled)
                chosen=type;
            if(disabled){
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s",effectUnavailable
                        ?"Requires OnRegionEnter subject"
                        :"Create a Map variable before adding this node");
                ImGui::EndDisabled();
            }
        }
        ImGui::EndCombo();
        if(!chosen.empty())refresh(app,ed.addTriggerNode(project,condition,chosen));
    }
}
template<class App>
void globals(App& app){
    auto& ed=app.editor;
    auto& project=app.project;
    if(!ImGui::CollapsingHeader("Map variables (shared trigger state)",
                              ImGuiTreeNodeFlags_DefaultOpen))return;
    ImGui::TextDisabled("Signed integers, -100000000 to +100000000; reset each match.");
    const auto it=project.extensions.find("trigger_variables");
    if(it!=project.extensions.end() && it->second.template is<picojson::object>()){
        // Copy: app reload can replace the source project during UI callbacks.
        const auto variables=it->second.template get<picojson::object>();
        static std::unordered_map<std::string,int> drafts;
        for(const auto& entry:variables){
            const std::string& name=entry.first;
            const int current=static_cast<int>(entry.second.template get<double>());
            ImGui::PushID(name.c_str());
            ImGui::TextUnformatted(name.c_str());
            ImGui::SameLine();
            int& value=drafts.emplace(name,current).first->second;
            ImGui::SetNextItemWidth(135.f);
            ImGui::InputInt("Initial", &value);
            const bool committed=ImGui::IsItemDeactivatedAfterEdit();
            const bool active=ImGui::IsItemActive();
            if(committed){
                refresh(app,ed.setTriggerVariable(project,name,value));
                value=current;
            }else if(!active)value=current;
            ImGui::SameLine();
            if(ImGui::SmallButton("Remove")){
                refresh(app,ed.removeTriggerVariable(project,name));
            }
            ImGui::SameLine();
            static std::string renameDraft;
            if(ImGui::SmallButton("Rename")){
                renameDraft=name;
                ImGui::OpenPopup("Rename script global");
            }
            if(ImGui::BeginPopup("Rename script global")){
                ImGui::TextUnformatted("Rename and migrate all typed ECA references");
                ImGui::InputText("New ID",&renameDraft);
                if(ImGui::Button("Apply rename") &&
                   ed.renameTriggerVariable(project,name,renameDraft)){
                    refresh(app,true);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    static std::string newName;
    ImGui::InputText("New variable ID", &newName);
    ImGui::SameLine();
    if(ImGui::SmallButton("Add variable")){
        if(ed.addTriggerVariable(project,newName,0)){
            newName.clear();
            refresh(app,true);
        }
    }
}
template<class App>
void draw(App& app){
    auto& ed=app.editor;
    auto& project=app.project;
    const auto count=ed.triggerCount(project);
    ImGui::Text("%zu map rules",count);
    globals(app);
    if(!count){ImGui::TextDisabled("Use New trigger in the Browser to begin.");return;}
    const auto rule=ed.triggerObject(project,ed.selectedTrigger%count);
    const auto idIt=rule.find("id");
    const std::string id=idIt!=rule.end() && idIt->second.template is<std::string>()
        ?idIt->second.template get<std::string>():"<unnamed>";
    ImGui::TextWrapped("Rule: %s",id.c_str());
    if(ImGui::SmallButton("Open playtest monitor"))
        app.triggerTraceVisible=true;
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
                                  "OnRegionEnter","MineralsBelow","OnVariableChanged"}){
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
