#pragma once
#include "EditorSession.hpp"
#include "imgui.h"
#include <algorithm>
#include <exception>
#include <string>
#include <vector>

// UI-only adapter over EditorSession's validated field transactions.
// The authoring/runtime and reference lists are independent of Dear ImGui.
namespace CreatorReferenceWidgets {
template<class App>
bool commit(App& app,const std::string& field,const picojson::array& next){
    if(!app.editor.setCatalogField(app.project,field,picojson::value(next)))return false;
    try{app.reloadFromProject();}
    catch(const std::exception& e)
        {app.editor.message=std::string("CATALOG RELOAD FAILED: ")+e.what();}
    return true;
}
template<class App>
bool draw(App& app,const std::string& parentId,const std::string& field,
          const picojson::value& value){
    auto& ed=app.editor;
    auto& project=app.project;
    const std::string category=field=="trainable_units"?"Unit":"Effect";
    if(!value.template is<picojson::array>())return false;
    const picojson::array items=value.template get<picojson::array>();
    const std::size_t limit=field=="trainable_units"?12:8;
    ImGui::SeparatorText(category=="Unit"?"Trainable units":"Effect sequence children");
    ImGui::TextDisabled("%zu / %zu references | ordered",items.size(),limit);
    for(std::size_t index=0;index<items.size();++index){
        if(!items[index].template is<std::string>()){
            ImGui::TextDisabled("Non-string entry: edit as JSON");
            continue;
        }
        const std::string target=items[index].template get<std::string>();
        ImGui::PushID(static_cast<int>(index));
        ImGui::TextWrapped("%zu. %s",index+1,target.c_str());
        if(project.catalog.find(target)){
            ImGui::SameLine();
            if(ImGui::SmallButton("Open")){
                if(ed.selectCatalogId(project,target)){
                    ImGui::PopID();
                    return true;
                }
            }
        }
        if(index>0){
            ImGui::SameLine();
            if(ImGui::SmallButton("Up")){
                picojson::array next=items;
                std::swap(next[index],next[index-1]);
                if(commit(app,field,next)){ImGui::PopID();return true;}
            }
        }
        if(index+1<items.size()){
            ImGui::SameLine();
            if(ImGui::SmallButton("Down")){
                picojson::array next=items;
                std::swap(next[index],next[index+1]);
                if(commit(app,field,next)){ImGui::PopID();return true;}
            }
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("Remove")){
            picojson::array next=items;
            next.erase(next.begin()+static_cast<std::ptrdiff_t>(index));
            if(commit(app,field,next)){ImGui::PopID();return true;}
        }
        ImGui::PopID();
    }
    if(items.size()>=limit){
        ImGui::TextDisabled("Maximum references reached");
        return false;
    }
    // Resolve candidates before a mutation; never iterate project catalog
    // storage across a successful replace/undo of a project document.
    std::vector<std::string> candidates;
    for(const auto& candidate:project.catalog.entries()){
        if(candidate.type!=category || candidate.id==parentId)continue;
        if(std::any_of(items.begin(),items.end(),[&](const picojson::value& item){
             return item.is<std::string>() && item.get<std::string>()==candidate.id;}))
            continue;
        if(category=="Unit"){
            const auto resolved=project.catalog.resolvedFields(candidate.id);
            const auto structure=resolved.find("is_structure");
            if(structure!=resolved.end() && structure->second.template is<bool>() &&
               structure->second.template get<bool>())continue;
        }
        candidates.push_back(candidate.id);
    }
    const std::string prompt="Choose "+category;
    if(ImGui::BeginCombo("Add reference",prompt.c_str())){
        std::string selected;
        for(const auto& id:candidates)
            if(ImGui::Selectable(id.c_str()))selected=id;
        ImGui::EndCombo();
        if(!selected.empty()){
            picojson::array next=items;
            next.emplace_back(selected);
            if(commit(app,field,next))return true;
        }
    }
    ImGui::TextDisabled("Invalid/duplicate/cyclic references are rejected on commit");
    return false;
}
} // namespace CreatorReferenceWidgets
