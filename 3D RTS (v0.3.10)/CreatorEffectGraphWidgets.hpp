#pragma once
#include "DataCatalog.hpp"
#include "imgui.h"
#include <algorithm>
#include <string>

// Visual inspection of a validated Effect DAG. The graph is displayed as a
// bounded, navigable tree; all writes remain in EditorSession transactions.
// No renderer, GLFW, OS or simulation state is accessed by this GUI adapter.
namespace CreatorEffectGraphWidgets {
inline std::string node(const DataCatalog& catalog,const std::string& id,
                        int depth,int& visited){
    if(depth>10 || ++visited>64)return {};
    const auto fields=catalog.resolvedFields(id);
    const auto kindIt=fields.find("kind");
    const std::string kind=kindIt!=fields.end() && kindIt->second.is<std::string>()?
                           kindIt->second.get<std::string>():"Unknown";
    ImGui::PushID(id.c_str());
    const bool sequence=kind=="Sequence";
    if(depth==0)ImGui::SetNextItemOpen(true,ImGuiCond_Once);
    const bool expanded=ImGui::TreeNodeEx("##graphNode",
        ImGuiTreeNodeFlags_SpanAvailWidth|
        (!sequence?ImGuiTreeNodeFlags_Leaf:0),
        "%s | %s",kind.c_str(),id.c_str());
    ImGui::SameLine();
    const bool open=ImGui::SmallButton("Edit");
    std::string chosen=open?id:"";
    if(expanded){
        if(sequence){
            const auto children=fields.find("children");
            if(children!=fields.end() && children->second.is<picojson::array>()){
                const auto& list=children->second.get<picojson::array>();
                for(std::size_t index=0;index<list.size();++index){
                    if(!list[index].is<std::string>())continue;
                    ImGui::PushID(static_cast<int>(index));
                    const auto clicked=node(catalog,list[index].get<std::string>(),
                                            depth+1,visited);
                    ImGui::PopID();
                    if(chosen.empty())chosen=clicked;
                }
            }
        }else if(kind=="ApplyBehavior" || kind=="RemoveBehavior"){
            const auto ref=fields.find("behavior");
            if(ref!=fields.end() && ref->second.is<std::string>()){
                const auto b=compileBehavior(catalog,ref->second.get<std::string>());
                ImGui::TextDisabled("Status: %s  |  %u ticks  |  %d stacks",
                    b.id.c_str(),b.durationTicks,b.maxStacks);
            }
        }else {
            const auto amount=fields.find("amount");
            if(amount!=fields.end() && amount->second.is<double>())
                ImGui::TextDisabled("Magnitude %.2f",amount->second.get<double>());
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
    return chosen;
}
inline std::string draw(const DataCatalog& catalog,const std::string& root){
    const auto operations=compileEffect(catalog,root);
    ImGui::TextDisabled("Validated operations: %zu | bounded graph depth and size",
                        operations.size());
    int visited=0;
    return node(catalog,root,0,visited);
}
}
