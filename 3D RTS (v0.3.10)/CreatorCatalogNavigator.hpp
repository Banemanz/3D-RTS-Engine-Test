#pragma once
#include "RTSCatalogNavigator.hpp"
#include "EditorSession.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// Galaxy-style navigation *in concept* only: an original visual projection
// of this project's typed references, with no borrowed UI assets or code.
namespace CreatorCatalogNavigator {
inline bool visible=true;
template<class App>
void draw(App& app){
    if(!visible || app.editor.panel!=EditorSession::Panel::Data)return;
    const auto* selected=app.editor.selected(app.project);
    const auto screen=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(320.f,58.f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(
        std::min(310.f,std::max(235.f,screen.x*.25f)),
        std::min(450.f,std::max(270.f,screen.y-90.f))),
        ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Creator | Data Navigator",&visible)){
        ImGui::End();return;
    }
    if(!selected){
        ImGui::TextWrapped("Select a catalog object to inspect its dependencies and uses.");
        ImGui::End();return;
    }
    const std::string id=selected->id;
    ImGui::TextDisabled("%s | %s",selected->type.c_str(),id.c_str());
    ImGui::SeparatorText("Jump to catalog object");
    static std::string search;
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##catalog_jump","Search by ID or type",&search);
    if(!search.empty()){
        std::string needle=search;
        std::transform(needle.begin(),needle.end(),needle.begin(),
                       [](unsigned char c){return char(std::tolower(c));});
        int shown=0;
        for(const auto& entry:app.project.catalog.entries()){
            std::string label=entry.type+" "+entry.id;
            std::transform(label.begin(),label.end(),label.begin(),
                           [](unsigned char c){return char(std::tolower(c));});
            if(label.find(needle)==std::string::npos)continue;
            ImGui::PushID(entry.id.c_str());
            const std::string text=entry.type+" | "+entry.id;
            if(ImGui::Selectable(text.c_str(),id==entry.id)){
                app.editor.selectCatalogId(app.project,entry.id);
                search.clear();
                ImGui::PopID();ImGui::End();return;
            }
            ImGui::PopID();
            if(++shown>=12)break;
        }
        if(shown==0)ImGui::TextDisabled("No matching catalog objects");
        ImGui::Separator();
    }
    const auto edges=rtsCatalogLinks(app.project,id);
    int upstream=0,downstream=0;
    for(const auto& link:edges){
        if(link.from==id)++upstream;
        if(link.to==id)++downstream;
    }
    ImGui::TextDisabled("%d dependencies  |  %d references",upstream,downstream);
    const auto list=[&](const char* heading,bool outgoing){
        ImGui::SeparatorText(heading);
        int displayed=0;
        for(const auto& edge:edges){
            if(outgoing?edge.from!=id:edge.to!=id)continue;
            const std::string& dest=outgoing?edge.to:edge.from;
            const auto* target=app.project.catalog.find(dest);
            if(!target)continue;
            ImGui::PushID((edge.from+"."+edge.field+"."+edge.to).c_str());
            const std::string label=target->type+" | "+dest;
            if(ImGui::Selectable(label.c_str(),false)){
                app.editor.selectCatalogId(app.project,dest);
                ImGui::PopID();return true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("[%s%s]",edge.field.c_str(),
                                edge.inherited?" inherited":"");
            ImGui::PopID();
            if(++displayed>=40){
                ImGui::TextDisabled("More references are available in project data");
                break;
            }
        }
        if(displayed==0)ImGui::TextDisabled("None");
        return false;
    };
    if(list("Depends on",true)){ImGui::End();return;}
    if(list("Used by",false)){ImGui::End();return;}
    ImGui::Separator();
    ImGui::TextWrapped("References are read-only. Click an entry to navigate; edit it in the F7 Inspector. Inherited links are marked explicitly.");
    ImGui::End();
}
}
