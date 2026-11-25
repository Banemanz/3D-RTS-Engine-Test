#pragma once
#include "EditorSession.hpp"
#include "TriggerTree.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <string>
#include <unordered_map>
#include <vector>

// Presentation-only recursive tree. All edits pass through EditorSession's
// transactional project validator; no pointer into live project survives a
// refresh, and no gameplay logic or OS-specific code runs in this widget.
namespace CreatorTriggerTreeWidgets {
template<class App> void refresh(App& app,bool changed){
    if(!changed)return;
    try{app.reloadFromProject();}
    catch(const std::exception& e){
        app.editor.message=std::string("TRIGGER RELOAD FAILED: ")+e.what();
    }
}
inline std::string identity(const TriggerTreePath& path,const std::string& suffix=""){
    std::string value;
    for(const auto& step:path)
        value+="/"+step.list+":"+std::to_string(step.index);
    return value+"/"+suffix;
}
inline std::vector<std::string> globals(const RTSProject& project){
    std::vector<std::string> result;
    const auto variables=project.extensions.find("trigger_variables");
    if(variables!=project.extensions.end() && variables->second.is<picojson::object>())
        for(const auto& value:variables->second.get<picojson::object>())
            result.push_back(value.first);
    return result;
}
inline std::vector<std::string> propertyOptions(const RTSProject& project,
                                                 const std::string& key){
    if(key=="operator")return {"==","!=","<","<=",">",">="};
    if(key=="result")return {"Victory","Defeat"};
    if(key=="variable")return globals(project);
    std::vector<std::string> choices;
    if(key=="region")
        for(const auto& item:project.placements)
            if(item.kind=="Region")choices.push_back(item.id);
    if(key=="catalog" || key=="effect")
        for(const auto& item:project.catalog.entries())
            if(item.type==(key=="effect"?"Effect":"Unit"))
                choices.push_back(item.id);
    return choices;
}
template<class App>
void scalar(App& app,const TriggerTreePath& path,
            const std::string& key,const picojson::value& value){
    auto& project=app.project;
    auto& ed=app.editor;
    ImGui::PushID((identity(path,key)).c_str());
    if(value.is<std::string>()){
        const auto current=value.get<std::string>();
        if(key=="catalog" || key=="effect" || key=="result" ||
           key=="region" || key=="variable" || key=="operator"){
            if(ImGui::BeginCombo(key.c_str(),current.c_str())){
                for(const auto& option:propertyOptions(project,key))
                    if(ImGui::Selectable(option.c_str(),option==current))
                        refresh(app,ed.setTriggerTreeField(
                            project,path,key,picojson::value(option)));
                ImGui::EndCombo();
            }
        }else{
            static std::unordered_map<std::string,std::string> drafts;
            auto& draft=drafts.emplace(identity(path,key),current).first->second;
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText(key.c_str(),&draft);
            const bool commit=ImGui::IsItemDeactivatedAfterEdit();
            const bool active=ImGui::IsItemActive();
            if(commit){
                refresh(app,ed.setTriggerTreeField(
                    project,path,key,picojson::value(draft)));
                draft=current;
            }else if(!active)draft=current;
        }
    }else if(value.is<double>()){
        const double current=value.get<double>();
        static std::unordered_map<std::string,double> drafts;
        double& draft=drafts.emplace(identity(path,key),current).first->second;
        ImGui::SetNextItemWidth(-1);
        ImGui::InputDouble(key.c_str(),&draft,0,0,"%.3f");
        const bool commit=ImGui::IsItemDeactivatedAfterEdit();
        const bool active=ImGui::IsItemActive();
        if(commit){
            refresh(app,ed.setTriggerTreeField(
                project,path,key,picojson::value(draft)));
            draft=current;
        }else if(!active)draft=current;
    }else if(value.is<bool>()){
        bool current=value.get<bool>();
        if(ImGui::Checkbox(key.c_str(),&current))
            refresh(app,ed.setTriggerTreeField(
                project,path,key,picojson::value(current)));
    }
    ImGui::PopID();
}
inline const char* const* operations(){
    static const char* ops[]={"Literal","Variable","Add","Subtract",
                              "Multiply","Divide","Modulo","Min","Max",
                              "Negate","Absolute","Clamp"};
    return ops;
}
template<class App>
void expression(App& app,const TriggerTreePath& path,const std::string& key,
                const picojson::value& value,std::vector<std::string> exprPath,
                int depth){
    if(depth>8 || !value.is<picojson::object>())return;
    const auto tree=value.get<picojson::object>();
    const auto opIt=tree.find("op");
    if(opIt==tree.end() || !opIt->second.is<std::string>())return;
    const auto op=opIt->second.get<std::string>();
    const std::string id=identity(path,key)+":"+std::to_string(depth)+
        "/"+([&](){std::string v;for(const auto& p:exprPath)v+="/"+p;return v;}());
    ImGui::PushID(id.c_str());
    const bool opened=ImGui::TreeNodeEx(op.c_str(),ImGuiTreeNodeFlags_DefaultOpen);
    if(opened){
        if(ImGui::BeginCombo("Operation",op.c_str())){
            for(std::size_t n=0;n<12;++n){
                const std::string chosen=operations()[n];
                const auto names=globals(app.project);
                const bool unavailable=chosen=="Variable" && names.empty();
                if(unavailable)ImGui::BeginDisabled();
                if(ImGui::Selectable(chosen.c_str(),chosen==op) && !unavailable &&
                   chosen!=op)
                    refresh(app,app.editor.setTriggerExpressionNode(
                        app.project,path,key,exprPath,
                        TriggerTree::defaultExpression(chosen,
                            names.empty()?"":names.front())));
                if(unavailable)ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        if(op=="Literal"){
            const auto field=tree.find("value");
            if(field!=tree.end() && field->second.is<double>()){
                const int current=static_cast<int>(field->second.get<double>());
                static std::unordered_map<std::string,int> drafts;
                int& number=drafts.emplace(id,current).first->second;
                ImGui::InputInt("Integer", &number);
                const bool commit=ImGui::IsItemDeactivatedAfterEdit();
                const bool active=ImGui::IsItemActive();
                if(commit){
                    refresh(app,app.editor.setTriggerExpressionNode(
                        app.project,path,key,exprPath,
                        RTSScriptExpression::number(number)));
                    number=current;
                }else if(!active)number=current;
            }
        }else if(op=="Variable"){
            const auto field=tree.find("name");
            if(field!=tree.end() && field->second.is<std::string>()){
                const auto current=field->second.get<std::string>();
                if(ImGui::BeginCombo("Map variable",current.c_str())){
                    for(const auto& name:globals(app.project))
                        if(ImGui::Selectable(name.c_str(),name==current))
                            refresh(app,app.editor.setTriggerExpressionNode(
                                app.project,path,key,exprPath,
                                RTSScriptExpression::variable(name)));
                    ImGui::EndCombo();
                }
            }
        }else{
            const bool unary=op=="Negate" || op=="Absolute";
            const bool clamp=op=="Clamp";
            const std::vector<std::string> slots=unary
                ?std::vector<std::string>{"operand"}
                :clamp?std::vector<std::string>{"value","minimum","maximum"}
                :std::vector<std::string>{"left","right"};
            for(const auto& slot:slots){
                const auto child=tree.find(slot);
                if(child==tree.end())continue;
                ImGui::TextDisabled("%s",slot.c_str());
                auto nested=exprPath;
                nested.push_back(slot);
                expression(app,path,key,child->second,nested,depth+1);
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}
template<class App>
void drawList(App& app,const picojson::object& rule,
              const TriggerTreePath& parent,const std::string& list,int depth){
    if(depth>5)return;
    auto& ed=app.editor;
    const bool condition=list=="conditions";
    const char* label=list=="then"?"Then (on true)":
                      list=="else"?"Else (on false)":
                      condition?"Conditions (AND)":
                      list=="actions"?"Actions (ordered)":"Trigger nodes";
    ImGui::PushID((identity(parent,list)).c_str());
    ImGui::SeparatorText(label);
    picojson::array items;
    try{items=TriggerTree::listSnapshot(rule,parent,list);}
    catch(const std::exception& e){
        ImGui::TextDisabled("%s",e.what());ImGui::PopID();return;
    }
    for(std::size_t i=0;i<items.size();++i){
        if(!items[i].is<picojson::object>())continue;
        const auto node=items[i].get<picojson::object>();
        const auto typeIt=node.find("type");
        const std::string kind=typeIt!=node.end() &&
            typeIt->second.is<std::string>()?
            typeIt->second.get<std::string>():"Unknown";
        TriggerTreePath path=parent;
        path.push_back({list,i});
        ImGui::PushID(static_cast<int>(i));
        const std::string header=std::to_string(i+1)+". "+kind;
        if(ImGui::TreeNodeEx(header.c_str(),ImGuiTreeNodeFlags_DefaultOpen)){
            if(i>0 && ImGui::SmallButton("Up"))
                refresh(app,ed.moveTriggerTreeNode(app.project,path,-1));
            if(i>0)ImGui::SameLine();
            if(i+1<items.size() && ImGui::SmallButton("Down"))
                refresh(app,ed.moveTriggerTreeNode(app.project,path,1));
            if(i+1<items.size())ImGui::SameLine();
            if(ImGui::SmallButton("Duplicate"))
                refresh(app,ed.duplicateTriggerTreeNode(app.project,path));
            ImGui::SameLine();
            if(ImGui::SmallButton("Delete"))
                refresh(app,ed.removeTriggerTreeNode(app.project,path));
            for(const auto& field:node){
                if(field.first=="type" || field.first=="conditions" ||
                   field.first=="then" || field.first=="else" ||
                   field.first=="actions")continue;
                if(field.first=="value_expr" || field.first=="amount_expr" ||
                   field.first=="times_expr"){
                    ImGui::TextDisabled("%s",field.first.c_str());
                    expression(app,path,field.first,field.second,{},0);
                }else scalar(app,path,field.first,field.second);
            }
            const bool value=(condition && kind=="VariableCompare") ||
                  (!condition && (kind=="GrantMinerals" ||
                                  kind=="SetVariable" || kind=="AddVariable" ||
                                  kind=="Repeat"));
            if(value){
                const bool repeat=kind=="Repeat";
                const char* exprKey=repeat?"times_expr":
                    kind=="GrantMinerals"?"amount_expr":"value_expr";
                const bool active=node.count(exprKey)>0;
                if(ImGui::SmallButton(active?"Use fixed number":"Use formula")){
                    refresh(app,ed.setTriggerTreeOperand(app.project,path,!active));
                }
                if(!active && !globals(app.project).empty()){
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Use map variable"))
                        refresh(app,ed.setTriggerTreeOperand(
                            app.project,path,true,globals(app.project).front()));
                }
            }
            if(kind=="IfThenElse"){
                drawList(app,rule,path,"conditions",depth+1);
                drawList(app,rule,path,"then",depth+1);
                drawList(app,rule,path,"else",depth+1);
            }else if(kind=="Repeat")
                drawList(app,rule,path,"actions",depth+1);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    const std::vector<std::string> options=condition
        ?std::vector<std::string>{"PlayerMineralsAtLeast","UnitCountAtLeast",
                                  "UnitTypeCountAtLeast","SubjectOwnerIs","VariableCompare"}
        :std::vector<std::string>{"ShowMessage","GrantMinerals","SpawnUnit",
                                  "ApplyEffect","EndMatch","SetVariable",
                                  "AddVariable","IfThenElse","Repeat"};
    const char* caption=condition?"Add condition...":"Add action...";
    if(ImGui::BeginCombo("Add node",caption)){
        std::string event;
        auto eventIt=rule.find("event");
        if(eventIt!=rule.end() && eventIt->second.is<picojson::object>()){
            const auto& object=eventIt->second.get<picojson::object>();
            const auto type=object.find("type");
            if(type!=object.end() && type->second.is<std::string>())
                event=type->second.get<std::string>();
        }
        for(const auto& candidate:options){
            const bool needsGlobal=(candidate=="VariableCompare" ||
                                    candidate=="SetVariable" ||
                                    candidate=="AddVariable") &&
                                    globals(app.project).empty();
            const bool invalidEffect=candidate=="ApplyEffect" &&
                                     event!="OnRegionEnter";
            const bool disabled=needsGlobal || invalidEffect;
            if(disabled)ImGui::BeginDisabled();
            if(ImGui::Selectable(candidate.c_str()) && !disabled)
                refresh(app,ed.addTriggerTreeNode(
                    app.project,parent,list,candidate));
            if(disabled)ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
}
} // namespace CreatorTriggerTreeWidgets
