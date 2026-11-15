#pragma once
#include "RTSAreaShape.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <vector>

// F7 lightweight read-only preview of the same headless shape geometry used
// by the simulation and the in-world targeting outline. The preview has no
// GL state, project writes, persistent allocations or platform dependencies.
namespace CreatorAreaShapePreview {
inline void draw(RTSAreaShape shape,float rawRadius,
                 float rawAngle,float rawWidth){
    const float radius=std::isfinite(rawRadius)?
        std::clamp(rawRadius,.5f,24.f):4.f;
    const float angle=std::isfinite(rawAngle)?
        std::clamp(rawAngle,10.f,170.f):90.f;
    const float width=std::isfinite(rawWidth)?
        std::clamp(rawWidth,.25f,12.f):2.f;
    const float available=ImGui::GetContentRegionAvail().x;
    const float canvasWidth=std::max(110.f,std::min(430.f,available));
    constexpr float canvasHeight=166.f;
    ImGui::TextDisabled("Area footprint | caster on the left, forward +X");
    ImGui::PushID("area_shape_2d_preview");
    ImGui::InvisibleButton("##area_footprint",
                           ImVec2(canvasWidth,canvasHeight));
    const ImVec2 minimum=ImGui::GetItemRectMin();
    ImDrawList* list=ImGui::GetWindowDrawList();
    const ImVec2 maximum{minimum.x+canvasWidth,
                         minimum.y+canvasHeight};
    const auto background=ImGui::GetColorU32(ImVec4(.08f,.12f,.16f,1.f));
    const auto grid=ImGui::GetColorU32(ImVec4(.23f,.28f,.34f,.65f));
    const auto fill=ImGui::GetColorU32(ImVec4(.17f,.58f,.74f,.28f));
    const auto edge=ImGui::GetColorU32(ImVec4(.30f,.86f,.95f,.95f));
    const auto forwardColor=ImGui::GetColorU32(ImVec4(.87f,.72f,.35f,.9f));
    const auto hit=ImGui::GetColorU32(ImVec4(.37f,.88f,.57f,1.f));
    const auto miss=ImGui::GetColorU32(ImVec4(.92f,.38f,.38f,1.f));
    list->AddRectFilled(minimum,maximum,background,5.f);
    for(float x=minimum.x+20.f;x<maximum.x;x+=20.f)
        list->AddLine(ImVec2(x,minimum.y),ImVec2(x,maximum.y),grid,.6f);
    for(float y=minimum.y+20.f;y<maximum.y;y+=20.f)
        list->AddLine(ImVec2(minimum.x,y),ImVec2(maximum.x,y),grid,.6f);
    // Fit even a wide 170-degree cone without clipping. Coordinates are
    // normalized to the authored maximum reach, not the current camera.
    const float lateral=shape==RTSAreaShape::Line?width*.5f:radius;
    const float scale=std::min((canvasWidth*.58f)/radius,
                    (canvasHeight*.39f)/std::max(lateral,.5f));
    const ImVec2 center(minimum.x+canvasWidth*.31f,
                        minimum.y+canvasHeight*.50f);
    const auto transform=[&](double x,double z)->ImVec2{
        return ImVec2(center.x+float(x)*scale,
                      center.y-float(z)*scale);
    };
    const ImVec2 caster=transform(-radius*.22f,0.);
    list->AddLine(caster,center,forwardColor,2.f);
    list->AddCircleFilled(caster,4.f,forwardColor);
    if(shape==RTSAreaShape::Circle){
        list->AddCircleFilled(center,radius*scale,fill,64);
        list->AddCircle(center,radius*scale,edge,64,2.f);
    }else{
        const auto outline=rtsAreaOutline(shape,0.,0.,1.,0.,
                                          radius,angle,width);
        if(outline.size()>=4){
            std::vector<ImVec2> points;
            points.reserve(outline.size());
            // Last point repeats the first to close the contour.
            for(std::size_t i=0;i+1<outline.size();++i)
                points.push_back(transform(outline[i].x,outline[i].z));
            if(points.size()>=3)
                list->AddConvexPolyFilled(points.data(),
                                         int(points.size()),fill);
            for(std::size_t i=0;i<points.size();++i)
                list->AddLine(points[i],points[(i+1)%points.size()],edge,2.f);
        }
    }
    list->AddCircleFilled(center,3.f,edge);
    // Model test targets are informational only; they do not issue orders.
    // Classification calls exactly the same pure predicate as live effects.
    struct Sample {float x,z;};
    const Sample targets[]={
        {radius*.56f,0.f},{radius*.32f,radius*.45f},
        {-radius*.26f,0.f},{radius*.83f,width*.55f}
    };
    for(const auto& target:targets){
        const bool valid=rtsAreaContains(shape,target.x,target.z,
                                        1.,0.,radius,angle,width);
        const auto pos=transform(target.x,target.z);
        list->AddCircleFilled(pos,3.5f,valid?hit:miss);
    }
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Green sample dots are inside; red dots are outside. "
                          "The live engine checks actual Unit centers at impact.");
    ImGui::PopID();
    ImGui::TextDisabled("Green: affected  |  Red: outside  |  Preview only");
}
}
