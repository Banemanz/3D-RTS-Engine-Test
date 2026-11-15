#pragma once
#include "RTSTechGraph.hpp"
#include "EditorSession.hpp"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>

// A dockable, clickable TECHNOLOGY MAP backed by the same headless typed
// reference/Requirement projection used by RTSProjectTool --tech-graph.
// Viewing the graph never changes gameplay or edits the serialized project.
namespace CreatorTechGraph {
inline bool visible=false;
inline bool focusSelection=false;

inline ImU32 edgeColor(RTSTechLinkKind kind){
    switch(kind){
        case RTSTechLinkKind::Research:return IM_COL32(100,213,188,225);
        case RTSTechLinkKind::Train:return IM_COL32(109,178,255,210);
        case RTSTechLinkKind::Equip:return IM_COL32(187,149,242,210);
        case RTSTechLinkKind::Show:return IM_COL32(244,194,99,235);
        case RTSTechLinkKind::Use:return IM_COL32(126,222,143,235);
        case RTSTechLinkKind::Modifies:return IM_COL32(158,169,185,190);
    }
    return IM_COL32(210,210,210,210);
}
inline ImU32 nodeFill(const RTSTechNode& n){
    if(n.type=="Upgrade")return IM_COL32(35,72,79,245);
    if(n.type=="Ability")return IM_COL32(59,43,79,245);
    if(n.producer)return IM_COL32(49,65,85,245);
    return IM_COL32(47,55,68,245);
}
inline ImVec2 cubicPoint(ImVec2 a,ImVec2 b,ImVec2 c,ImVec2 d,float t){
    const float u=1.f-t;
    return ImVec2(u*u*u*a.x+3.f*u*u*t*b.x+3.f*u*t*t*c.x+t*t*t*d.x,
                  u*u*u*a.y+3.f*u*u*t*b.y+3.f*u*t*t*c.y+t*t*t*d.y);
}
inline void drawTechCurve(ImDrawList* draw,ImVec2 a,ImVec2 b,ImVec2 c,
                          ImVec2 d,ImU32 color,bool dashed){
    if(!dashed){
        draw->AddBezierCubic(a,b,c,d,color,2.f);
        return;
    }
    // Show negation/disjunction as actual dashes, not just a thin solid line.
    for(int i=0;i<40;++i)if((i/3)%2==0)
        draw->AddLine(cubicPoint(a,b,c,d,float(i)/40.f),
                      cubicPoint(a,b,c,d,float(i+1)/40.f),color,1.7f);
}
template<class App>
void draw(App& app){
    if(!visible || app.editor.panel!=EditorSession::Panel::Data)return;
    const auto graph=rtsBuildTechGraph(app.project);
    int maxRows=0;
    for(int rows:graph.rowCount)maxRows=std::max(maxRows,rows);
    const auto screen=ImGui::GetIO().DisplaySize;
    const float left=std::min(350.f,screen.x*.27f);
    ImGui::SetNextWindowPos(ImVec2(left,60.f),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(std::max(560.f,screen.x-left-24.f),
        std::max(360.f,std::min(screen.y-100.f,240.f+94.f*maxRows))),
        ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Creator | Technology Map",&visible)){
        ImGui::End();return;
    }
    const auto* selected=app.editor.selected(app.project);
    const std::string focus=selected?selected->id:"";
    std::set<std::string> shown;
    const bool focused=focusSelection && !focus.empty() &&
        std::any_of(graph.nodes.begin(),graph.nodes.end(),
                    [&](const RTSTechNode& n){return n.id==focus;});
    if(focused){
        shown.insert(focus);
        for(const auto& e:graph.links)if(e.from==focus || e.to==focus){
            shown.insert(e.from);shown.insert(e.to);
        }
    }
    ImGui::TextDisabled("%zu objects  |  %zu typed links  |  original authoring data",
                        graph.nodes.size(),graph.links.size());
    ImGui::SameLine();
    ImGui::Checkbox("Focus selected",&focusSelection);
    ImGui::SameLine();
    if(ImGui::SmallButton("Copy Graphviz DOT"))
        ImGui::SetClipboardText(rtsTechGraphDot(graph).c_str());
    if(graph.truncated)ImGui::TextColored(ImVec4(1.f,.55f,.32f,1.f),
        "Large project: projection truncated at 512 nodes / 2048 links");
    ImGui::TextWrapped("Solid lines are typed links; dashed NOT / ANY marks Boolean context. Show and Use connections are references inside Requirement trees, not proof of a mandatory prerequisite.");
    const std::array<RTSTechLinkKind,6> legend{{
        RTSTechLinkKind::Research,RTSTechLinkKind::Train,
        RTSTechLinkKind::Equip,RTSTechLinkKind::Show,
        RTSTechLinkKind::Use,RTSTechLinkKind::Modifies
    }};
    for(std::size_t i=0;i<legend.size();++i){
        const auto kind=legend[i];
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(edgeColor(kind)),
                           "%s",rtsTechLinkName(kind));
        if(i+1<legend.size())ImGui::SameLine();
    }
    ImGui::Separator();

    constexpr float ColPitch=237.f,RowPitch=94.f,BoxW=202.f,BoxH=66.f;
    constexpr float PadX=24.f,PadY=44.f;
    const char* headers[4]={"PRODUCERS","RESEARCH","UNITS","ABILITIES"};
    const ImVec2 fullSize(PadX*2+ColPitch*3+BoxW,
                          PadY*2+RowPitch*std::max(maxRows,1));
    if(ImGui::BeginChild("##technology_canvas",ImVec2(0.f,0.f),
                         ImGuiChildFlags_Borders,
                         ImGuiWindowFlags_HorizontalScrollbar)){
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        auto* drawList=ImGui::GetWindowDrawList();
        for(int col=0;col<4;++col)
            drawList->AddText(ImVec2(origin.x+PadX+col*ColPitch,
                                     origin.y+8.f),
                              IM_COL32(171,187,205,240),headers[col]);

        std::map<std::string,ImVec2> position;
        for(const auto& n:graph.nodes){
            if(focused && !shown.count(n.id))continue;
            position[n.id]=ImVec2(origin.x+PadX+n.column*ColPitch,
                                  origin.y+PadY+n.row*RowPitch);
        }
        // Edges are behind interactive nodes. Same-column links and self
        // references (e.g. hide an Upgrade at its own max level) are legal.
        for(const auto& edge:graph.links){
            const auto first=position.find(edge.from),last=position.find(edge.to);
            if(first==position.end() || last==position.end())continue;
            const ImVec2 a(first->second.x+BoxW,first->second.y+BoxH*.5f);
            const ImVec2 b(last->second.x,last->second.y+BoxH*.5f);
            const auto col=edgeColor(edge.kind);
            if(edge.from==edge.to){
                const ImVec2 start(a.x-23.f,a.y-BoxH*.5f);
                const ImVec2 end(a.x-60.f,a.y-BoxH*.5f);
                drawTechCurve(drawList,start,ImVec2(start.x+15.f,start.y-34.f),
                    ImVec2(end.x-5.f,end.y-34.f),end,col,
                    edge.negated || edge.anyBranch);
            }else{
                const float delta=std::max(42.f,std::abs(b.x-a.x)*.38f);
                const bool reverse=b.x<=a.x;
                const ImVec2 cp1(a.x+(reverse?32.f:delta),a.y-(reverse?36.f:0.f));
                const ImVec2 cp2(b.x-(reverse?32.f:delta),b.y-(reverse?36.f:0.f));
                drawTechCurve(drawList,a,cp1,cp2,b,col,
                              edge.negated || edge.anyBranch);
            }
            if(edge.kind==RTSTechLinkKind::Show ||
               edge.kind==RTSTechLinkKind::Use){
                // Small midpoint tag distinguishes logical variants without
                // replacing the actual Requirement graph authoring controls.
                const std::string tag=(edge.negated?"NOT ":"")+
                    std::string(edge.anyBranch?"ANY ":"")+
                    (edge.threshold>0?std::to_string(edge.threshold):"");
                if(!tag.empty())
                    drawList->AddText(ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f-10.f),
                                      col,tag.c_str());
            }
        }
        for(const auto& n:graph.nodes){
            const auto it=position.find(n.id);
            if(it==position.end())continue;
            const ImVec2 at=it->second;
            const ImVec2 end(at.x+BoxW,at.y+BoxH);
            const bool isSelected=n.id==focus;
            drawList->AddRectFilled(at,end,nodeFill(n),7.f);
            drawList->AddRect(at,end,isSelected?
                IM_COL32(255,218,120,255):IM_COL32(96,117,142,245),
                7.f,0,isSelected?2.2f:1.f);
            drawList->PushClipRect(ImVec2(at.x+8.f,at.y+4.f),
                                   ImVec2(end.x-7.f,end.y-3.f),true);
            drawList->AddText(ImVec2(at.x+9.f,at.y+8.f),
                IM_COL32(238,244,250,255),n.title.c_str());
            drawList->AddText(ImVec2(at.x+9.f,at.y+29.f),
                IM_COL32(178,194,211,250),n.id.c_str());
            const std::string detail=n.type+
                (n.maxLevel>0?"  |  "+std::to_string(n.maxLevel)+" levels":"")+
                (n.otherGates>0?"  |  +"+std::to_string(n.otherGates)+" other gates":"");
            drawList->AddText(ImVec2(at.x+9.f,at.y+46.f),
                              IM_COL32(145,210,191,250),detail.c_str());
            drawList->PopClipRect();

            ImGui::PushID(n.id.c_str());
            ImGui::SetCursorScreenPos(at);
            ImGui::InvisibleButton("node",ImVec2(BoxW,BoxH));
            if(ImGui::IsItemHovered()){
                ImGui::BeginTooltip();
                ImGui::Text("%s | %s",n.type.c_str(),n.id.c_str());
                int count=0;
                for(const auto& e:graph.links){
                    if(e.from!=n.id && e.to!=n.id)continue;
                    if(++count>16){ImGui::TextDisabled("More links in DOT export");break;}
                    ImGui::Text("%s -> %s : %s%s%s",
                        e.from.c_str(),e.to.c_str(),rtsTechLinkName(e.kind),
                        e.negated?" NOT":"",e.anyBranch?" ANY":"");
                    if(e.threshold>0)ImGui::TextDisabled("    Threshold: %d",e.threshold);
                    if(!e.requirementId.empty())
                        ImGui::TextDisabled("    Requirement: %s",e.requirementId.c_str());
                }
                ImGui::Separator();
                ImGui::TextDisabled("Click to open catalog inspector");
                ImGui::EndTooltip();
            }
            if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
                app.editor.selectCatalogId(app.project,n.id);
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x+fullSize.x,
                                         origin.y+fullSize.y));
        ImGui::Dummy(ImVec2(1.f,1.f)); // establishes both scroll extents
    }
    ImGui::EndChild();
    ImGui::End();
}
}
