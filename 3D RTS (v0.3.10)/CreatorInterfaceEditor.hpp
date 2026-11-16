#pragma once
#include "EditorSession.hpp"
#include "RTSHUDLayout.hpp"
#include "RTSHUDProfile.hpp"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// The genuine authored *in-game* Interface Editor. Unlike appearance settings,
// this edits map metadata consumed by the native gameplay HUD renderer AND
// the player's click-hit router. The canvas is an engine-layout projection:
// no proprietary Galaxy assets, XML, shaders or imported UI sprites.
namespace CreatorInterfaceEditor {
inline int previewResolution=0;
inline int selectedPanel=0;
inline constexpr std::array<const char*,5> panelNames={{
    "Minimap","Command grid","Selection dossier","Bottom panel","Resource strip"
}};
inline constexpr std::array<std::array<int,2>,3> resolutions={{
    {{1280,720}},{{1920,1080}},{{800,600}}
}};

inline ImU32 fill(int index){
    switch(index){
        case 0:return IM_COL32(24,105,117,245);
        case 1:return IM_COL32(36,79,141,245);
        case 2:return IM_COL32(51,67,109,245);
        case 3:return IM_COL32(13,36,63,246);
        case 4:return IM_COL32(23,68,96,246);
    }
    return IM_COL32(15,35,55,255);
}
template<class App>
void draw(App& app){
    static std::string projectId;
    static std::string committed;
    static RTSHUDProfile draft;
    static bool dirty=false;
    const auto live=RTSHUDProfile::fromProject(app.project);
    const std::string id=live.toJson().serialize();
    if(projectId!=app.projectPath || (!dirty && committed!=id)){
        projectId=app.projectPath;committed=id;draft=live;dirty=false;
    }
    ImGui::TextColored(ImVec4(.48f,.85f,.92f,1.f),
        "INTERFACE DESIGNER  /  Native gameplay HUD");
    ImGui::SameLine();
    ImGui::TextDisabled("%s",dirty?"UNAPPLIED PREVIEW":"PROJECT LAYOUT");
    ImGui::TextWrapped("Select an element in the preview or the property list. Drag the minimap or command grid to alter its edge inset, the bottom panel to change its height, and the resource bar to move its top edge. Apply commits one validated undoable map edit.");
    ImGui::Separator();
    const char* views[]={"Desktop 1280 x 720","Full HD 1920 x 1080",
                        "Compact 800 x 600"};
    if(ImGui::BeginCombo("Preview screen",views[previewResolution])){
        for(int i=0;i<3;++i)
            if(ImGui::Selectable(views[i],previewResolution==i))
                previewResolution=i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Editor zoom: %.0f%%",app.uiEffectiveScale*100.f);

    const int w=resolutions[previewResolution][0];
    const int h=resolutions[previewResolution][1];
    const RTSHUDLayout layout(w,h,draft);
    const ImVec2 available=ImGui::GetContentRegionAvail();
    const float ui=std::max(.8f,ImGui::GetFontSize()/16.5f);
    const float sideWidth=std::clamp(305.f*ui,235.f,450.f*ui);
    const float previewWidth=std::max(210.f,available.x-sideWidth-18.f);
    const float previewHeight=std::max(220.f,std::min(
        available.y-34.f,610.f*ui));
    if(ImGui::BeginTable("##InterfaceCanvasGrid",2,
       ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV|
       ImGuiTableFlags_ScrollX|ImGuiTableFlags_SizingStretchProp,
       ImVec2(0,previewHeight+10.f),
       previewWidth+sideWidth+10.f)){
        ImGui::TableSetupColumn("Canvas",ImGuiTableColumnFlags_WidthStretch,
                                previewWidth);
        ImGui::TableSetupColumn("Layout controls",ImGuiTableColumnFlags_WidthFixed,
                                sideWidth);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("LIVE GEOMETRY  |  %.0f x %.0f",
                            float(w),float(h));
        const ImVec2 canvasAvail=ImGui::GetContentRegionAvail();
        const float k=std::min((canvasAvail.x-14.f)/float(w),
            (previewHeight-45.f)/float(h));
        const ImVec2 size=ImVec2(float(w)*k,float(h)*k);
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const ImVec2 center(origin.x+(canvasAvail.x-size.x)*.5f,origin.y+5.f);
        const auto px=[&](float x){return center.x+x*k;};
        const auto py=[&](float y){return center.y+y*k;};
        const auto rect=[&](const HUDRect& r,ImU32 c,
                            bool selected,const char* label){
            const ImVec2 lo(px(r.x),py(r.y)),hi(px(r.x+r.w),py(r.y+r.h));
            ImDrawList* d=ImGui::GetWindowDrawList();
            d->AddRectFilled(lo,hi,c,3.f*ui);
            d->AddRect(lo,hi,selected?IM_COL32(147,240,238,255):
                             IM_COL32(83,150,187,225),
                       3.f*ui,0,selected?2.7f:1.f);
            if(r.w*k>85.f && r.h*k>24.f)
                d->AddText(ImVec2(lo.x+5.f*ui,lo.y+4.f*ui),
                           IM_COL32(234,243,255,255),label);
        };
        ImGui::GetWindowDrawList()->AddRectFilled(center,
            ImVec2(center.x+size.x,center.y+size.y),
            IM_COL32(12,28,46,255),2.f*ui);
        // Draw bottom first, then the real contained boxes, then top strip.
        rect(layout.bottom,fill(3),selectedPanel==3,"BOTTOM HUD");
        rect(layout.selectionArea,fill(2),selectedPanel==2,"SELECTION");
        rect(layout.commandArea,fill(1),selectedPanel==1,"COMMAND GRID");
        rect(layout.minimap,fill(0),selectedPanel==0,"MINIMAP");
        rect(layout.topBar,fill(4),selectedPanel==4,"RESOURCES");
        for(int i=0;i<12;++i){
            const auto& grid=layout.commandArea;
            const float gap=6.f,cw=(grid.w-12.f)/3.f;
            const float ch=(grid.h-18.f)/4.f;
            const float x=grid.x+(i%3)*(cw+gap);
            const float y=grid.y+(i/3)*(ch+gap);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(px(x+2),py(y+2)),
                ImVec2(px(x+cw-2),py(y+ch-2)),
                IM_COL32(69,126,183,215),2.f);
        }
        // Canvas interaction occurs through the same pixels derived from
        // layout.hit boxes, with no duplicated HUD geometry or fake selection.
        ImGui::SetCursorScreenPos(center);
        ImGui::InvisibleButton("##hudLayoutCanvas",size);
        const bool clicked=ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const ImVec2 mouse=ImGui::GetIO().MousePos;
        const float mx=(mouse.x-center.x)/k,my=(mouse.y-center.y)/k;
        if(clicked){
            if(layout.minimap.contains(mx,my))selectedPanel=0;
            else if(layout.commandArea.contains(mx,my))selectedPanel=1;
            else if(layout.selectionArea.contains(mx,my))selectedPanel=2;
            else if(layout.topBar.contains(mx,my))selectedPanel=4;
            else if(layout.bottom.contains(mx,my))selectedPanel=3;
        }
        if(ImGui::IsItemActive() &&
           ImGui::IsMouseDragging(ImGuiMouseButton_Left,3.f)){
            const ImVec2 delta=ImGui::GetIO().MouseDelta;
            const float dx=delta.x/k,dy=delta.y/k;
            const auto bounded=[](float v,float lo,float hi){
                return std::clamp(v,lo,hi);
            };
            if(selectedPanel==0)draft.minimapInset=bounded(
                draft.minimapInset+dx,8.f,48.f);
            if(selectedPanel==1)draft.commandInset=bounded(
                draft.commandInset-dx,8.f,48.f);
            if(selectedPanel==3)draft.bottomExtra=bounded(
                draft.bottomExtra-dy,-40.f,65.f);
            if(selectedPanel==4)draft.resourceTop=bounded(
                draft.resourceTop+dy,20.f,70.f);
            if(selectedPanel!=2)dirty=true;
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x,center.y+size.y+13.f*ui));
        ImGui::TextDisabled("Click to inspect; drag the selected element. Layout is generated by the real HUD geometry code.");

        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("ELEMENTS");
        for(int i=0;i<int(panelNames.size());++i){
            if(ImGui::Selectable(panelNames[static_cast<std::size_t>(i)],
                                  selectedPanel==i))
                selectedPanel=i;
        }
        ImGui::SeparatorText("Selected element");
        ImGui::TextColored(ImVec4(.51f,.88f,1.f,1.f),"%s",
             panelNames[static_cast<std::size_t>(selectedPanel)]);
        auto slider=[&](const char* label,float& value,float min,float max){
            ImGui::SetNextItemWidth(-1.f);
            if(ImGui::SliderFloat(label,&value,min,max,"%.1f"))
                dirty=true;
        };
        switch(selectedPanel){
            case 0:
                slider("Minimap scale",draft.minimapScale,.75f,1.3f);
                slider("Left inset",draft.minimapInset,8.f,48.f);
                break;
            case 1:
                slider("Command width",draft.commandWidth,260.f,390.f);
                slider("Right inset",draft.commandInset,8.f,48.f);
                break;
            case 2:
                ImGui::TextWrapped("The selection dossier fills the safe interval between the minimap and command grid. Resize those neighbors to change the available width.");
                break;
            case 3:
                slider("Bottom strip height offset",draft.bottomExtra,-40.f,65.f);
                ImGui::TextDisabled("Adaptive baseline: %d px",h<620?160:194);
                break;
            case 4:
                slider("Resource strip top",draft.resourceTop,20.f,70.f);
                slider("Resource strip height",draft.resourceHeight,36.f,72.f);
                break;
        }
        ImGui::SeparatorText("Operations");
        if(ImGui::Button("Apply layout",ImVec2(-1.f,0))){
            if(app.editor.setHUDProfile(app.project,draft)){
                try{app.reloadFromProject();}
                catch(const std::exception& e)
                    {app.editor.message=std::string("HUD RELOAD FAILED: ")+e.what();}
            }
            committed=RTSHUDProfile::fromProject(app.project).toJson().serialize();
            dirty=false;
        }
        if(ImGui::Button("Discard preview",ImVec2(-1.f,0))){
            draft=live;dirty=false;committed=id;
        }
        if(ImGui::Button("Reset to classic HUD",ImVec2(-1.f,0))){
            draft=RTSHUDProfile{};dirty=true;
        }
        if(dirty)ImGui::TextColored(ImVec4(1.f,.77f,.35f,1.f),
                                   "Preview not yet saved");
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Engine connection");
    ImGui::TextWrapped("Apply commits one project-wide undo step. F9 and the packaged game use these authored rectangles for both rendering and hit testing. This is the first gameplay-HUD layout designer, not yet a complete freeform widget/skin scripting system.");
    if(!app.editor.message.empty())ImGui::TextWrapped("%s",app.editor.message.c_str());
}
}