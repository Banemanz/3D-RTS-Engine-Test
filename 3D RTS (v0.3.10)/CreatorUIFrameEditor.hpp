#pragma once
#include "EditorSession.hpp"
#include "RTSUIFrameLayout.hpp"
#include "RTSUIEditorGeometry.hpp"
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
inline bool revealSelectedFrame=false;
inline bool snapToGrid=true;
inline bool drawGrid=true;
inline int gridStep=8;
inline const std::array<std::array<int,2>,3> resolutions={{
    {{1280,720}},{{1920,1080}},{{800,600}}
}};
inline const char* kinds[]={"Panel","Label","Button","Bar"};
inline const char* anchors[]={"TopLeft","TopRight","BottomLeft","BottomRight","Center"};
inline const char* palettes[]={"Navy","Teal","Cobalt","Amber","Slate"};
inline const char* bindings[]={"None","Minerals","Supply","Workers",
                                "Selected","SelectedHealth","Message","Ticks","Variable"};
inline const char* actions[]={"None","SelectArmy","SelectWorkers","FocusBase","Trigger"};
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
        if(findFrame(draft,focusFrameId)>=0){
            selected=focusFrameId;
            revealSelectedFrame=true;
        }
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
    ImGui::Checkbox("Snap to grid",&snapToGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Grid lines",&drawGrid);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(112.f*ui);
    const char* gridOptions[]={"4 px","8 px","16 px","32 px"};
    const int steps[]={4,8,16,32};
    if(ImGui::BeginCombo("Grid step",
                         gridStep==4?"4 px":gridStep==16?"16 px":
                         gridStep==32?"32 px":"8 px")){
        for(int i=0;i<4;++i)
            if(ImGui::Selectable(gridOptions[i],gridStep==steps[i]))
                gridStep=steps[i];
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
        if(drawGrid){
            // Limit rendered grid density on heavily downscaled previews.
            // Logical grid step remains exact; only the visual guide
            // spacing increases to keep it readable at 125-250% DPI.
            const int skip=std::max(1,int(std::ceil(11.f/(gridStep*k))));
            const int guideStep=gridStep*skip;
            drawList->PushClipRect(base,canvasMax,true);
            for(int x=guideStep;x<w;x+=guideStep)
                drawList->AddLine(ImVec2(px(float(x)),base.y),
                                  ImVec2(px(float(x)),canvasMax.y),
                                  IM_COL32(72,117,152,44),1.f);
            for(int y=guideStep;y<h;y+=guideStep)
                drawList->AddLine(ImVec2(base.x,py(float(y))),
                                  ImVec2(canvasMax.x,py(float(y))),
                                  IM_COL32(72,117,152,44),1.f);
            drawList->PopClipRect();
        }
        drawList->AddRect(base,canvasMax,IM_COL32(75,119,156,230));
        // Build a real, nested frame tree from the project geometry model.
        for(const auto& item:resolved){
            if(item.clipped.w<1.f||item.clipped.h<1.f)continue;
            const auto& box=item.clipped;
            const ImVec2 lo{px(box.x),py(box.y)};
            const ImVec2 hi{px(box.x+box.w),py(box.y+box.h)};
            if(item.source.kind!="Label")
                drawList->AddRectFilled(lo,hi,item.visible?
                    rgb(item.source.palette):IM_COL32(27,37,53,130),3.f*ui);
            const bool chosen=item.source.id==selected;
            drawList->AddRect(lo,hi,
                chosen?IM_COL32(133,241,229,255):
                item.visible?IM_COL32(67,141,180,225):
                             IM_COL32(126,132,145,110),
                3.f*ui,0,chosen?3.f:1.f);
            if(item.source.kind=="Bar" && item.visible){
                const float r=rtsUIFrameBarRatio(item.source,app.game);
                const HUDRect track{item.bounds.x+3,item.bounds.y+3,
                    (item.bounds.w-6)*r,item.bounds.h-6};
                const auto vis=rtsUIRectIntersect(track,item.clipped);
                if(vis.w>0.f && vis.h>0.f)
                    drawList->AddRectFilled(ImVec2(px(vis.x),py(vis.y)),
                        ImVec2(px(vis.x+vis.w),py(vis.y+vis.h)),
                        IM_COL32(53,200,187,236),2.f);
            }
            const auto name=item.visible?
                rtsUIFrameText(item.source,app.game):std::string{"HIDDEN"};
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
        // Active selection gets a high-contrast resize handle in
        // actual game-space coordinates.
        const auto selectedIt=std::find_if(resolved.begin(),resolved.end(),
            [&](const RTSUIResolvedFrame& item){return item.source.id==selected;});
        if(selectedIt!=resolved.end() && selectedIt->clipped.w>0 &&
           selectedIt->clipped.h>0){
            const auto& box=selectedIt->clipped;
            const ImVec2 handle{px(box.x+box.w),py(box.y+box.h)};
            const float hs=std::max(6.f,7.f*ui);
            drawList->AddRectFilled(
                ImVec2(handle.x-hs,handle.y-hs),handle,
                IM_COL32(126,236,222,255),1.5f);
            drawList->AddRect(ImVec2(handle.x-hs,handle.y-hs),handle,
                              IM_COL32(16,44,62,255),1.5f);
        }
        ImGui::SetCursorScreenPos(base);
        ImGui::InvisibleButton("##CustomFrameCanvas",size);
        const ImVec2 mouse=ImGui::GetIO().MousePos;
        const float mx=(mouse.x-base.x)/k,my=(mouse.y-base.y)/k;
        static std::string dragId;
        static float dragX=0,dragY=0,dragW=0,dragH=0;
        static bool resizing=false;
        static bool pressedFrame=false;
        if(ImGui::IsItemClicked()){
            const int found=rtsUIFrameAt(resolved,mx,my);
            pressedFrame=found>=0;
            if(found>=0){
                const auto& clicked=resolved[static_cast<std::size_t>(found)];
                selected=clicked.source.id;
                const float near=std::max(7.f,9.f*ui)/k;
                resizing=std::fabs(mx-(clicked.clipped.x+clicked.clipped.w))<=near &&
                         std::fabs(my-(clicked.clipped.y+clicked.clipped.h))<=near;
            }else resizing=false;
        }
        if(ImGui::IsItemActivated()){
            dragId=pressedFrame?selected:std::string{};
            const int index=findFrame(draft,dragId);
            if(index>=0){
                const auto& f=draft[static_cast<std::size_t>(index)];
                dragX=f.x;dragY=f.y;dragW=f.width;dragH=f.height;
            }
        }
        if(ImGui::IsItemActive() &&
           ImGui::IsMouseDragging(ImGuiMouseButton_Left,3.f)){
            const int index=findFrame(draft,dragId);
            if(index>=0){
                auto& f=draft[static_cast<std::size_t>(index)];
                const ImVec2 delta=ImGui::GetMouseDragDelta(
                    ImGuiMouseButton_Left,3.f);
                if(resizing){
                    f.width=rtsUISnapDimension(dragW+delta.x/k,gridStep,
                                               snapToGrid,24.f,650.f);
                    f.height=rtsUISnapDimension(dragH+delta.y/k,gridStep,
                                                snapToGrid,20.f,400.f);
                }else{
                    f.x=rtsUISnapCoordinate(dragX+delta.x/k,gridStep,snapToGrid);
                    f.y=rtsUISnapCoordinate(dragY+delta.y/k,gridStep,snapToGrid);
                }
                dirty=true;
            }
        }
        if(ImGui::IsItemHovered() && !ImGui::GetIO().WantTextInput){
            const int index=findFrame(draft,selected);
            if(index>=0){
                auto& f=draft[static_cast<std::size_t>(index)];
                const int nudge=snapToGrid?gridStep:1;
                const int speed=ImGui::GetIO().KeyShift?4:1;
                float dx=0.f,dy=0.f;
                if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))dx=-1.f;
                if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))dx=1.f;
                if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))dy=-1.f;
                if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))dy=1.f;
                if(dx!=0.f||dy!=0.f){
                    f.x=rtsUISnapCoordinate(f.x+dx*nudge*speed,gridStep,
                                            snapToGrid);
                    f.y=rtsUISnapCoordinate(f.y+dy*nudge*speed,gridStep,
                                            snapToGrid);
                    dirty=true;
                }
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
        const int chosenIndex=findFrame(draft,selected);
        if(chosenIndex>=0){
            const auto& chosen=draft[static_cast<std::size_t>(chosenIndex)];
            ImGui::TextColored(ImVec4(.53f,.91f,.96f,1.f),
                "%s | %s",chosen.id.c_str(),chosen.kind.c_str());
            if(chosen.action=="Trigger")
                ImGui::TextColored(ImVec4(1.f,.79f,.43f,1.f),
                    "Click: F6 trigger event");
        }
        ImGui::Separator();
        const float propertyHeight=std::max(165.f,panelHeight-85.f*ui);
        if(ImGui::BeginChild("##FramePropertiesScroll",ImVec2(0,propertyHeight),
                             ImGuiChildFlags_Borders)){
        // Fast frame chooser remains visible even when the full tree
        // is closed, making the property inspector useful at 150-250% UI
        // scaling rather than burying variable/visibility controls.
        ImGui::SetNextItemWidth(-1.f);
        if(ImGui::BeginCombo("##frameQuickSelect",
                              selected.empty()?"Select a frame":selected.c_str())){
            for(const auto& frame:draft){
                const std::string label=frame.kind+"  "+frame.id;
                if(ImGui::Selectable(label.c_str(),selected==frame.id)){
                    selected=frame.id;
                    revealSelectedFrame=true;
                }
            }
            ImGui::EndCombo();
        }
        if(ImGui::CollapsingHeader("Browse hierarchy (optional)")){
        const float listHeight=std::clamp(panelHeight*.30f,88.f,175.f);
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
                if(revealSelectedFrame && selected==frame.id){
                    ImGui::SetScrollHereY(.45f);
                    revealSelectedFrame=false;
                }
            }
        }
        ImGui::EndChild();
        } // optional full hierarchy
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
            // Primary Button action should be visible immediately at
            // high DPI; don't bury it behind several size controls.
            if(f.kind=="Button")combo("Click action",f.action,actions,5);
            ImGui::SeparatorText("Live data & conditional visibility");
            if(f.kind=="Bar"){
                const char* progress[]={"Supply","SelectedHealth","Variable"};
                combo("Progress value",f.binding,progress,3);
            }else combo("Live data binding",f.binding,bindings,9);
            // Live typed F6 map variable references are validated at
            // Apply time and migrated during variable renames.
            const auto globals=app.project.extensions.find("trigger_variables");
            const picojson::object* variables=globals!=app.project.extensions.end()
                && globals->second.template is<picojson::object>()?
                    &globals->second.template get<picojson::object>():nullptr;
            const auto chooseVariable=[&](const char* label,std::string& target,
                                          bool allowNone){
                ImGui::PushID(label);
                ImGui::TextUnformatted(label);
                ImGui::SetNextItemWidth(-1.f);
                if(ImGui::BeginCombo("##var",
                       target.empty()?"Always / none":target.c_str())){
                    if(allowNone && ImGui::Selectable("Always / none",
                                                       target.empty())){
                        target.clear();dirty=true;
                    }
                    if(variables)for(const auto& entry:*variables)
                        if(ImGui::Selectable(entry.first.c_str(),
                                             target==entry.first)){
                            target=entry.first;dirty=true;
                        }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            };
            if(f.binding=="Variable"){
                if(f.variable.empty() && variables && !variables->empty()){
                    f.variable=variables->begin()->first;dirty=true;
                }
                chooseVariable("Bound F6 map variable",f.variable,false);
                if(variables && variables->count(f.variable))
                    ImGui::TextDisabled("Current value: %d",
                                       rtsUIVariableValue(app.game,f.variable));
                if(f.kind=="Bar"){
                    ImGui::TextUnformatted("Progress minimum (integer)");
                    ImGui::SetNextItemWidth(-1.f);
                    if(ImGui::InputInt("##VariableBarMin",&f.barMin))dirty=true;
                    ImGui::TextUnformatted("Progress maximum (integer)");
                    ImGui::SetNextItemWidth(-1.f);
                    if(ImGui::InputInt("##VariableBarMax",&f.barMax))dirty=true;
                    f.barMin=std::clamp(f.barMin,-100000000,100000000);
                    f.barMax=std::clamp(f.barMax,-100000000,100000000);
                    if(f.barMax<=f.barMin)
                        ImGui::TextColored(ImVec4(1.f,.68f,.35f,1.f),
                                           "Maximum must exceed minimum");
                }
                if(!variables || variables->empty())
                    ImGui::TextWrapped("Create an F6 map variable first. Apply rejects missing bindings.");
            }else if(!f.variable.empty()){
                f.variable.clear();dirty=true;
            }
            ImGui::SeparatorText("Conditional visibility");
            chooseVariable("Visible when F6 variable reaches threshold",
                           f.visibleVariable,true);
            if(!f.visibleVariable.empty()){
                ImGui::TextUnformatted("Visible when variable >= value");
                ImGui::SetNextItemWidth(-1.f);
                if(ImGui::InputInt("##showAtLeast",&f.visibleAtLeast))dirty=true;
                f.visibleAtLeast=std::clamp(f.visibleAtLeast,-100000000,100000000);
                ImGui::TextDisabled("%s = %d",f.visibleVariable.c_str(),
                       rtsUIVariableValue(app.game,f.visibleVariable));
            }
            if(ImGui::CollapsingHeader("Geometry, anchors & palette")){
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
            HUDRect parentBounds{0,0,float(w),float(h)};
            bool canAlign=true;
            try{parentBounds=rtsUIFrameParentRect(resolved,f,w,h);}
            catch(const std::invalid_argument&){canAlign=false;}
            ImGui::SeparatorText("Align inside parent / screen");
            if(!canAlign)ImGui::TextColored(ImVec4(1.f,.68f,.35f,1.f),
                "Fix the frame parent before aligning");
            ImGui::BeginDisabled(!canAlign);
            const auto alignAxis=[&](int axis,int pos){
                rtsUIAlignFrame(f,parentBounds,axis,pos);
                dirty=true;
            };
            if(ImGui::SmallButton("Left"))alignAxis(0,0);
            ImGui::SameLine();
            if(ImGui::SmallButton("Center X"))alignAxis(0,1);
            ImGui::SameLine();
            if(ImGui::SmallButton("Right"))alignAxis(0,2);
            if(ImGui::SmallButton("Top"))alignAxis(1,0);
            ImGui::SameLine();
            if(ImGui::SmallButton("Middle"))alignAxis(1,1);
            ImGui::SameLine();
            if(ImGui::SmallButton("Bottom"))alignAxis(1,2);
            ImGui::EndDisabled();
            ImGui::TextDisabled("Arrow keys over canvas: nudge frame");
            ImGui::TextUnformatted("Text / optional binding prefix");
            ImGui::SetNextItemWidth(-1.f);
            if(ImGui::InputText("##frameText",&f.text))dirty=true;
            }
            if(f.kind=="Panel" || f.kind=="Button"){
                if(f.kind=="Button" && f.action=="Trigger" && !f.consumeInput){
                    f.consumeInput=true;dirty=true;
                }
                if(f.kind!="Button" || f.action!="Trigger"){
                    if(ImGui::Checkbox("Consume world mouse clicks",&f.consumeInput))
                        dirty=true;
                }else ImGui::TextDisabled(
                    "Script buttons always consume world clicks");
            }
            if(f.kind=="Button" && f.action=="Trigger")
                ImGui::TextWrapped(
                    "F6 Trigger Editor: choose OnUIFrameClicked and this frame ID. Clicks are replay-recorded at 60Hz.");
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
