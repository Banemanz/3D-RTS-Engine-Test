#pragma once
#include "RTSUIFrames.hpp"
#include "RTSHUDLayout.hpp"
#include <algorithm>
#include <cmath>
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
    for(const auto& f:frames){
        HUDRect base=root,clip=root;
        int depth=0;
        bool parentVisible=true;
        if(!f.parent.empty()){
            const auto it=std::find_if(out.begin(),out.end(),
                [&](const RTSUIResolvedFrame& parent){
                    return parent.source.id==f.parent;
                });
            if(it==out.end())throw std::invalid_argument("Missing UI parent");
            base=it->bounds;
            clip=it->clipped;
            depth=it->depth+1;
            parentVisible=it->visible;
        }
        float x=base.x,y=base.y;
        if(f.anchor=="TopRight" || f.anchor=="BottomRight")
            x+=base.w-f.width;
        else if(f.anchor=="Center")
            x+=(base.w-f.width)*.5f;
        if(f.anchor=="BottomLeft" || f.anchor=="BottomRight")
            y+=base.h-f.height;
        else if(f.anchor=="Center")
            y+=(base.h-f.height)*.5f;
        const HUDRect rect{x+f.x,y+f.y,f.width,f.height};
        const bool shown=parentVisible && (!game || rtsUIFrameVisible(f,*game));
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
