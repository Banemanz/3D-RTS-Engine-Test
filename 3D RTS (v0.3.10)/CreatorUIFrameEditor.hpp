#pragma once
#include "EditorSession.hpp"
#include "RTSUIFrameLayout.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace CreatorUIFrameEditor {
inline int previewResolution=0;
inline std::string focusFrameId; // deterministic CI/author selection
inline const std::array<std::array<int,2>,3> resolutions={{
    {{1280,720}},{{1920,1080}},{{800,600}}
}};
inline const char* kinds[]={"Panel","Label","Button","Bar"};
inline const char* anchors[]={"TopLeft","TopRight","BottomLeft","BottomRight","Center"};
inline const char* palettes[]={"Navy","Teal","Cobalt","Amber","Slate"};
inline const char* bindings[]={"None","Minerals","Supply","Workers",
                                "Selected","SelectedHealth","Message","Ticks"};
inline const char* actions[]={"None","SelectArmy","SelectWorkers","FocusBase"};
inline std::string nextId(const std::vector<RTSUIFrame>& frames){
    for(int i=1;i<=999;++i){
        const auto candidate="ui_"+std::to_string(i);
        if(std::none_of(frames.begin(),frames.end(),
           [&](const RTSUIFrame& f){return f.id==candidate;}))return candidate;
    }
    return {}; // unreachable with <=32
}
inline int findFrame(const std::vector<RTSUIFrame>& frames,
                     const std::string& id){
    for(std::size_t i=0;i<frames.size();++i)if(frames[i].id==id)return int(i);
    return -1;
}
inline void eraseSubtree(std::vector<RTSUIFrame>& frames,const std::string& id){
    std::set<std::string> removed{id};
    for(const auto& frame:frames)
        if(removed.count(frame.parent))removed.insert(frame.id);
    frames.erase(std::remove_if(frames.begin(),frames.end(),
        [&](const RTSUIFrame& f){return removed.count(f.id)>0;}),frames.end());
}
inline ImU32 rgb(const std::string& name){
    const auto c=rtsUIFrameColor(name);
    return IM_COL32(int(c.r*255.f),int(c.g*255.f),int(c.b*255.f),244);
}
template<class App>
void draw(App& app){
    static std::string ownerProject;
    static std::string committed;
    static std::vector<RTSUIFrame> draft;
    static std::string selected;
    static bool dirty=false;
    const auto current=rtsUIFramesFromProject(app.project);
    const std::string version=rtsUIFramesToJson(current).serialize();
    if(ownerProject!=app.projectPath || (!dirty && version!=committed)){
        ownerProject=app.projectPath;
        committed=version;
        draft=current;
        selected=draft.empty()?std::string{}:draft.front().id;
        dirty=false;
    }
    if(!focusFrameId.empty()){
        if(findFrame(draft,focusFrameId)>=0)selected=focusFrameId;
        focusFrameId.clear();
    }
    const auto persist=[&]{
        if(app.editor.setUIFrames(app.project,draft)){
            try{app.reloadFromProject();}
            catch(const std::exception& e){
                app.editor.message=std::string("UI RELOAD FAILED: ")+e.what();
            }
            committed=rtsUIFramesToJson(draft).serialize();
            dirty=false;
        }
    };
    const auto revert=[&]{
        draft=current;committed=version;dirty=false;
        selected=draft.empty()?std::string{}:draft.front().id;
    };
    ImGui::TextColored(ImVec4(.54f,.88f,.97f,1.f),
                       "CUSTOM FRAMES  /  Hierarchical UI authoring");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu/32 frames %s",draft.size(),
                        dirty?"| UNAPPLIED":"| SAVED");
    ImGui::TextWrapped("Create screen-anchored panels, text, live data, buttons and bars. Parent/child nesting clips visible content and mouse input. Changes become live only after Apply.");
    const float ui=std::max(.9f,ImGui::GetFontSize()/16.5f);
    const auto add=[&](const char* type){
        if(draft.size()>=32)return;
        RTSUIFrame item;
        item.id=nextId(draft);
        item.kind=type;
        item.palette=item.kind=="Button"?"Cobalt":
                     item.kind=="Bar"?"Teal":"Navy";
        item.width=item.kind=="Panel"?250.f:150.f;
        item.height=item.kind=="Panel"?190.f:40.f;
        item.text=item.kind=="Panel"?"MISSION CONSOLE":
                  item.kind=="Button"?"SELECT ARMY":
                  item.kind=="Bar"?"HEALTH":"NEW LABEL";
        if(item.kind=="Bar")item.binding="SelectedHealth";
        if(item.kind=="Button"){
            item.action="SelectArmy";item.consumeInput=true;
        }
        if(item.kind=="Panel")item.consumeInput=true;
        const int parent=findFrame(draft,selected);
        if(parent>=0 && draft[static_cast<std::size_t>(parent)].kind=="Panel"){
            item.parent=draft[static_cast<std::size_t>(parent)].id;
            item.x=12.f;item.y=35.f;
        }else{
            item.x=30.f+18.f*float(draft.size()%6);
            item.y=85.f+27.f*float(draft.size()%6);
        }
        selected=item.id;
        draft.push_back(std::move(item));
        dirty=true;
    };
    if(ImGui::SmallButton("+ Panel"))add("Panel");
    ImGui::SameLine();
    if(ImGui::SmallButton("+ Label"))add("Label");
    ImGui::SameLine();
    if(ImGui::SmallButton("+ Button"))add("Button");
    ImGui::SameLine();
    if(ImGui::SmallButton("+ Bar"))add("Bar");
    ImGui::SameLine();
    const char* screens[]={"1280 x 720","1920 x 1080","800 x 600"};
    ImGui::SetNextItemWidth(195.f*ui);
    if(ImGui::BeginCombo("Preview size",screens[previewResolution])){
        for(int i=0;i<3;++i)
            if(ImGui::Selectable(screens[i],i==previewResolution))
                previewResolution=i;
        ImGui::EndCombo();
    }

    const int w=resolutions[previewResolution][0];
    const int h=resolutions[previewResolution][1];
    const ImVec2 avail=ImGui::GetContentRegionAvail();
    const float detailWidth=std::clamp(280.f*ui,240.f,440.f*ui);
    const float canvasWidth=std::max(240.f,avail.x-detailWidth-22.f);
    const float panelHeight=std::max(360.f,std::min(avail.y-24.f,640.f*ui));
    std::vector<RTSUIResolvedFrame> resolved;
    std::string error;
    try{resolved=rtsResolveUIFrames(draft,w,h);}
    catch(const std::exception& e){error=e.what();}
    if(ImGui::BeginTable("##FrameWorkspace",2,
         ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV|
         ImGuiTableFlags_ScrollX|ImGuiTableFlags_SizingStretchProp,
         ImVec2(0,panelHeight+6.f),canvasWidth+detailWidth+10.f)){
        ImGui::TableSetupColumn("Live frame preview",
            ImGuiTableColumnFlags_WidthStretch,canvasWidth);
        ImGui::TableSetupColumn("Hierarchy and properties",
            ImGuiTableColumnFlags_WidthFixed,detailWidth);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("SCREEN FRAME  |  %d x %d  |  Click or drag a frame",w,h);
        const ImVec2 space=ImGui::GetContentRegionAvail();
        const float k=std::max(.08f,std::min((space.x-16.f)/float(w),
                                      (panelHeight-48.f)/float(h)));
        const ImVec2 size{w*k,h*k};
        const ImVec2 original=ImGui::GetCursorScreenPos();
        const ImVec2 base{original.x+std::max(0.f,(space.x-size.x)*.5f),
                          original.y+5.f};
        auto px=[&](float x){return base.x+x*k;};
        auto py=[&](float y){return base.y+y*k;};
        const ImVec2 canvasMax{base.x+size.x,base.y+size.y};
        auto* drawList=ImGui::GetWindowDrawList();
        drawList->AddRectFilled(base,canvasMax,IM_COL32(13,29,50,255));
        drawList->AddRect(base,canvasMax,IM_COL32(75,119,156,230));
        // Build a real, nested frame tree from the project geometry model.
        for(const auto& item:resolved){
            if(item.clipped.w<1.f||item.clipped.h<1.f)continue;
            const auto& box=item.clipped;
            const ImVec2 lo{px(box.x),py(box.y)};
            const ImVec2 hi{px(box.x+box.w),py(box.y+box.h)};
            if(item.source.kind!="Label")
                drawList->AddRectFilled(lo,hi,rgb(item.source.palette),3.f*ui);
            const bool chosen=item.source.id==selected;
            drawList->AddRect(lo,hi,
                chosen?IM_COL32(133,241,229,255):IM_COL32(67,141,180,225),
                3.f*ui,0,chosen?3.f:1.f);
            if(item.source.kind=="Bar"){
                const float r=rtsUIFrameBarRatio(item.source,app.game);
                const HUDRect track{item.bounds.x+3,item.bounds.y+3,
                    (item.bounds.w-6)*r,item.bounds.h-6};
                const auto vis=rtsUIRectIntersect(track,item.clipped);
                if(vis.w>0.f && vis.h>0.f)
                    drawList->AddRectFilled(ImVec2(px(vis.x),py(vis.y)),
                        ImVec2(px(vis.x+vis.w),py(vis.y+vis.h)),
                        IM_COL32(53,200,187,236),2.f);
            }
            const auto name=rtsUIFrameText(item.source,app.game);
            if(!name.empty() && (hi.x-lo.x)>35.f && (hi.y-lo.y)>15.f){
                drawList->PushClipRect(lo,hi,true);
                // Native ImGui font stays crisp but is sized to the
                // logical GAME canvas zoom, not to the editor's own
                // 125-250% DPI multiplier. Avoid overlapping child labels.
                const float canvasFont=std::clamp(
                    ImGui::GetFontSize()*k*1.12f,8.f,ImGui::GetFontSize());
                drawList->AddText(ImGui::GetFont(),canvasFont,
                    ImVec2(lo.x+5.f,lo.y+3.f),
                    IM_COL32(237,247,255,255),name.c_str());
                drawList->PopClipRect();
            }
        }
        ImGui::SetCursorScreenPos(base);
        ImGui::InvisibleButton("##CustomFrameCanvas",size);
        const ImVec2 mouse=ImGui::GetIO().MousePos;
        const float mx=(mouse.x-base.x)/k,my=(mouse.y-base.y)/k;
        if(ImGui::IsItemClicked()){
            const int found=rtsUIFrameAt(resolved,mx,my);
            if(found>=0)selected=resolved[static_cast<std::size_t>(found)].source.id;
        }
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left,3.f)){
            const int index=findFrame(draft,selected);
            if(index>=0){
                auto& f=draft[static_cast<std::size_t>(index)];
                f.x=std::clamp(f.x+ImGui::GetIO().MouseDelta.x/k,-2048.f,2048.f);
                f.y=std::clamp(f.y+ImGui::GetIO().MouseDelta.y/k,-2048.f,2048.f);
                dirty=true;
            }
        }
        if(!error.empty())ImGui::TextColored(ImVec4(1.f,.68f,.37f,1.f),
                                               "Frame error: %s",error.c_str());
        ImGui::TableSetColumnIndex(1);
        // Keep Apply/Discard available at every font scale while the
        // property inspector itself scrolls independently on small screens.
        if(ImGui::SmallButton("Apply"))persist();
        ImGui::SameLine();
        if(ImGui::SmallButton("Discard"))revert();
        ImGui::SameLine();
        if(ImGui::SmallButton("Clear")){
            draft.clear();selected.clear();dirty=true;
        }
        if(dirty)ImGui::TextColored(ImVec4(1.f,.77f,.35f,1.f),
                                    "Unapplied changes");
        ImGui::Separator();
        const float propertyHeight=std::max(165.f,panelHeight-100.f*ui);
        if(ImGui::BeginChild("##FramePropertiesScroll",ImVec2(0,propertyHeight),
                             ImGuiChildFlags_Borders)){
        ImGui::TextDisabled("FRAME HIERARCHY");
        const float listHeight=std::max(110.f,155.f*ui);
        if(ImGui::BeginChild("##frameTree",ImVec2(0,listHeight),
                             ImGuiChildFlags_Borders)){
            for(const auto& frame:draft){
                const int idx=findFrame(draft,frame.id);
                int depth=0;
                if(idx>=0 && idx<int(resolved.size()))
                    depth=resolved[static_cast<std::size_t>(idx)].depth;
                const auto title=std::string(static_cast<std::size_t>(depth)*2,' ')+
                    frame.kind+"  "+frame.id;
                if(ImGui::Selectable(title.c_str(),selected==frame.id))
                    selected=frame.id;
            }
        }
        ImGui::EndChild();
        int index=findFrame(draft,selected);
        if(index>=0){
            RTSUIFrame& f=draft[static_cast<std::size_t>(index)];
            ImGui::SeparatorText("Frame properties");
            ImGui::TextColored(ImVec4(.52f,.90f,.96f,1.f),
                "%s  /  %s",f.id.c_str(),f.kind.c_str());
            const auto combo=[&](const char* label,std::string& current,
                                 const char* const* options,int count){
                ImGui::TextUnformatted(label);
                ImGui::SetNextItemWidth(-1.f);
                ImGui::PushID(label);
                if(ImGui::BeginCombo("##choice",current.c_str())){
                    for(int i=0;i<count;++i)
                        if(ImGui::Selectable(options[i],current==options[i])){
                            current=options[i];dirty=true;
                        }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            };
            combo("Anchor",f.anchor,anchors,5);
            combo("Blue theme palette",f.palette,palettes,5);
            ImGui::TextUnformatted("Parent (root or an earlier panel)");
            ImGui::SetNextItemWidth(-1.f);
            if(ImGui::BeginCombo("##parent",f.parent.empty()?"Screen":f.parent.c_str())){
                if(ImGui::Selectable("Screen",f.parent.empty())){
                    f.parent.clear();dirty=true;
                }
                for(int j=0;j<index;++j)
                    if(draft[static_cast<std::size_t>(j)].kind=="Panel"){
                        const auto& id=draft[static_cast<std::size_t>(j)].id;
                        if(ImGui::Selectable(id.c_str(),f.parent==id)){
                            f.parent=id;dirty=true;
                        }
                    }
                ImGui::EndCombo();
            }
            auto drag=[&](const char* label,float& value,float lo,float hi){
                ImGui::PushID(label);
                ImGui::TextUnformatted(label);
                ImGui::SetNextItemWidth(-1.f);
                if(ImGui::DragFloat("##value",&value,1.f,lo,hi,"%.0f px")){
                    value=std::clamp(value,lo,hi);dirty=true;
                }
                ImGui::PopID();
            };
            drag("X offset",f.x,-2048.f,2048.f);
            drag("Y offset",f.y,-2048.f,2048.f);
            drag("Width",f.width,24.f,650.f);
            drag("Height",f.height,20.f,400.f);
            ImGui::TextUnformatted("Text / optional binding prefix");
            ImGui::SetNextItemWidth(-1.f);
            if(ImGui::InputText("##frameText",&f.text))dirty=true;
            if(f.kind=="Bar"){
                const char* progress[]={"Supply","SelectedHealth"};
                combo("Progress value",f.binding,progress,2);
            }else combo("Live data binding",f.binding,bindings,8);
            if(f.kind=="Button")
                combo("Click action",f.action,actions,4);
            if(f.kind=="Panel" || f.kind=="Button"){
                if(ImGui::Checkbox("Consume world mouse clicks",&f.consumeInput))
                    dirty=true;
            }
            if(ImGui::Button("Duplicate frame",ImVec2(-1,0)) &&
               draft.size()<32){
                auto copy=f;
                copy.id=nextId(draft);
                copy.x+=20.f;copy.y+=20.f;
                selected=copy.id;
                draft.push_back(std::move(copy));
                dirty=true;
            }
            if(ImGui::Button("Delete frame and descendants",ImVec2(-1,0))){
                eraseSubtree(draft,f.id);dirty=true;
                selected=draft.empty()?std::string{}:draft.back().id;
            }
        }else ImGui::TextDisabled("Select or create a frame to edit.");
        if(!app.editor.message.empty())
            ImGui::TextWrapped("%s",app.editor.message.c_str());
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Rendering, hierarchy clipping and button hit-testing use one pure model. Only Buttons execute existing allowed RTS commands. No custom scripts, engine memory access or copied external game UI.");
}
} // namespace CreatorUIFrameEditor
