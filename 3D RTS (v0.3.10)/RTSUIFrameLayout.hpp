#pragma once
#include "RTSUIFrames.hpp"
#include "RTSHUDLayout.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

// Single pure geometric model for BOTH OpenGL presentation and real mouse
// handling. Nested frames always clip to all ancestors AND viewport bounds.
struct RTSUIResolvedFrame {
    RTSUIFrame source;
    HUDRect bounds;
    HUDRect clipped;
    int depth=0;
    bool visible=true; // inherited from parent and F6 trigger-variable state
};
struct RTSUIFrameHit {
    bool consumed=false;
    std::string action; // Empty or "None" consumes without issuing a command.
    std::string frameId;
};
inline HUDRect rtsUIRectIntersect(const HUDRect& a,const HUDRect& b){
    const float x0=std::max(a.x,b.x),y0=std::max(a.y,b.y);
    const float x1=std::min(a.x+a.w,b.x+b.w);
    const float y1=std::min(a.y+a.h,b.y+b.h);
    return {x0,y0,std::max(0.f,x1-x0),std::max(0.f,y1-y0)};
}
inline int rtsUIVariableValue(const RTSSkirmish& game,const std::string& name){
    const auto& variables=game.triggerDiagnostics().variables();
    const auto it=variables.find(name);
    return it==variables.end()?0:it->second;
}
inline bool rtsUIFrameVisible(const RTSUIFrame& frame,const RTSSkirmish& game){
    return frame.visibleVariable.empty() ||
           rtsUIVariableValue(game,frame.visibleVariable)>=frame.visibleAtLeast;
}
inline std::vector<RTSUIResolvedFrame> rtsResolveUIFrames(
    const std::vector<RTSUIFrame>& frames,int viewportWidth,int viewportHeight,
    const RTSSkirmish* game=nullptr){
    rtsValidateUIFrames(frames);
    const HUDRect root{0,0,float(std::max(1,viewportWidth)),
                                  float(std::max(1,viewportHeight))};
    std::vector<RTSUIResolvedFrame> out;
    out.reserve(frames.size());
    // The cursor belongs to each Panel ID (not global order). Nested flow
    // containers have independent cursors and stable author-defined order.
    std::map<std::string,float> flowCursor;
    struct WrapCursor { float x=0.f,y=0.f,rowHeight=0.f; bool occupied=false; };
    std::map<std::string,WrapCursor> wrapCursor;
    for(const auto& f:frames){
        HUDRect base=root,clip=root;
        int depth=0;
        bool parentVisible=true;
        const RTSUIResolvedFrame* parent=nullptr;
        if(!f.parent.empty()){
            const auto it=std::find_if(out.begin(),out.end(),
                [&](const RTSUIResolvedFrame& candidate){
                    return candidate.source.id==f.parent;
                });
            if(it==out.end())throw std::invalid_argument("Missing UI parent");
            parent=&*it;
            base=it->bounds;
            clip=it->clipped;
            depth=it->depth+1;
            parentVisible=it->visible;
        }
        const bool shown=parentVisible && (!game || rtsUIFrameVisible(f,*game));
        float x=base.x,y=base.y;
        float width=f.width,height=f.height;
        if(parent && parent->source.layout!="Free"){
            const auto& config=parent->source;
            const float padding=config.layoutPadding;
            if(config.layout=="WrapRow"){
                // Visible children pack in stable order; F6-hidden siblings
                // reserve no width, height or gap. Fine nudges are visual.
                if(f.stretchCrossAxis)
                    height=std::clamp(base.h-2.f*padding,20.f,400.f);
                auto& cursor=wrapCursor[f.parent];
                const float available=std::max(1.f,base.w-2.f*padding);
                if(shown && cursor.occupied &&
                   cursor.x+width>available+.001f){
                    cursor.x=0.f;
                    cursor.y+=cursor.rowHeight+config.layoutGap;
                    cursor.rowHeight=0.f;
                    cursor.occupied=false;
                }
                x+=padding+cursor.x+f.x;
                y+=padding+cursor.y+f.y;
                if(shown){
                    cursor.x+=width+config.layoutGap;
                    cursor.rowHeight=std::max(cursor.rowHeight,height);
                    cursor.occupied=true;
                }
            }else{
                const float cursor=flowCursor[f.parent];
                if(config.layout=="StackColumn"){
                    if(f.stretchCrossAxis)
                        width=std::clamp(base.w-2.f*padding,24.f,650.f);
                    x+=padding+f.x;
                    y+=padding+cursor+f.y;
                }else{ // StackRow
                    if(f.stretchCrossAxis)
                        height=std::clamp(base.h-2.f*padding,20.f,400.f);
                    x+=padding+cursor+f.x;
                    y+=padding+f.y;
                }
                if(shown)
                    flowCursor[f.parent]+=(
                        config.layout=="StackColumn"?height:width)+
                        config.layoutGap;
            }
        }else{
            // Leave every Free-layout coordinate byte-for-byte compatible
            // with the existing five-anchor authoring math.
            if(f.anchor=="TopRight" || f.anchor=="BottomRight")
                x+=base.w-width;
            else if(f.anchor=="Center")
                x+=(base.w-width)*.5f;
            if(f.anchor=="BottomLeft" || f.anchor=="BottomRight")
                y+=base.h-height;
            else if(f.anchor=="Center")
                y+=(base.h-height)*.5f;
            x+=f.x;
            y+=f.y;
        }
        const HUDRect rect{x,y,width,height};
        out.push_back({f,rect,rtsUIRectIntersect(clip,rect),depth,shown});
    }
    return out;
}
inline int rtsUIFrameAt(const std::vector<RTSUIResolvedFrame>& frames,
                       double x,double y){
    for(std::size_t i=frames.size();i>0;--i){
        const auto& item=frames[i-1];
        if(item.visible && item.clipped.w>0 && item.clipped.h>0 &&
           item.clipped.contains(x,y))
            return static_cast<int>(i-1);
    }
    return -1;
}
inline RTSUIFrameHit rtsUIFrameHit(
    const std::vector<RTSUIResolvedFrame>& frames,double x,double y){
    for(std::size_t i=frames.size();i>0;--i){
        const auto& f=frames[i-1];
        if(!f.visible || !f.source.consumeInput || !f.clipped.contains(x,y) ||
           f.clipped.w<=0 || f.clipped.h<=0)continue;
        return {true,f.source.kind=="Button"?f.source.action:"None",f.source.id};
    }
    return {};
}
inline float rtsUIFrameBarRatio(const RTSUIFrame& f,const RTSSkirmish& game){
    if(f.binding=="Variable"){
        const double value=static_cast<double>(rtsUIVariableValue(game,f.variable));
        const double denominator=static_cast<double>(f.barMax)-
                                 static_cast<double>(f.barMin);
        if(denominator<=0.)return 0.f; // rejected by project validation
        return static_cast<float>(std::clamp(
            (value-static_cast<double>(f.barMin))/denominator,0.,1.));
    }
    if(f.binding=="Supply")
        return game.supplyCap>0?
            std::clamp(float(game.supplyUsed)/float(game.supplyCap),0.f,1.f):0.f;
    if(f.binding=="SelectedHealth"){
        float current=0.f,maximum=0.f;
        for(const auto& unit:game.units)if(unit.ownerId==0 &&
                                            unit.isAlive() && unit.isSelected){
            current+=unit.currentHp;
            maximum+=unit.data.max_hp;
        }
        return maximum>0.f?std::clamp(current/maximum,0.f,1.f):0.f;
    }
    return 0.f;
}
inline std::string rtsUIFrameText(const RTSUIFrame& f,const RTSSkirmish& game){
    std::string data;
    if(f.binding=="Minerals")data=std::to_string(game.playerMinerals);
    else if(f.binding=="Supply")
        data=std::to_string(game.supplyUsed)+"/"+std::to_string(game.supplyCap);
    else if(f.binding=="Workers")data=std::to_string(game.activeWorkerCount());
    else if(f.binding=="Selected" || f.binding=="SelectedHealth"){
        int selected=0;
        for(const auto& u:game.units)
            if(u.ownerId==0 && u.isAlive() && u.isSelected)++selected;
        if(f.binding=="Selected")data=std::to_string(selected);
        else data=std::to_string(int(100.f*rtsUIFrameBarRatio(f,game)))+"%";
    }else if(f.binding=="Variable")
        data=std::to_string(rtsUIVariableValue(game,f.variable));
    else if(f.binding=="Message")data=game.triggerMessage;
    else if(f.binding=="Ticks")data=std::to_string(game.simulationTick());
    if(f.binding=="None")return f.text;
    if(data.empty())return f.text;
    return f.text.empty()?data:f.text+" "+data;
}
struct RTSUIFrameColor {float r=.09f,g=.21f,b=.31f;};
inline RTSUIFrameColor rtsUIFrameColor(const std::string& name){
    if(name=="Teal")return {.08f,.37f,.41f};
    if(name=="Cobalt")return {.12f,.27f,.53f};
    if(name=="Amber")return {.45f,.29f,.09f};
    if(name=="Slate")return {.22f,.29f,.35f};
    return {.06f,.13f,.23f};
}
