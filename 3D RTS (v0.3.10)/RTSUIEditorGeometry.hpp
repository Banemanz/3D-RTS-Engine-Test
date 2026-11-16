#pragma once
#include "RTSUIFrameLayout.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

// Headless GUI-layout math shared by editor drag handles, numeric controls
// and alignment commands. It never modifies engine gameplay state.
inline float rtsUISnapCoordinate(float value,int gridSize,bool enabled){
    if(!std::isfinite(value))return 0.f;
    if(gridSize!=4 && gridSize!=8 && gridSize!=16 && gridSize!=32)
        throw std::invalid_argument("Invalid UI editor grid step");
    const float clamped=std::clamp(value,-2048.f,2048.f);
    if(!enabled)return clamped;
    return std::clamp(std::round(clamped/float(gridSize))*float(gridSize),
                      -2048.f,2048.f);
}
inline float rtsUISnapDimension(float value,int gridSize,bool enabled,
                               float minimum,float maximum){
    if(!std::isfinite(value))return minimum;
    if(gridSize!=4 && gridSize!=8 && gridSize!=16 && gridSize!=32)
        throw std::invalid_argument("Invalid UI editor grid step");
    const float clamped=std::clamp(value,minimum,maximum);
    if(!enabled)return clamped;
    return std::clamp(std::round(clamped/float(gridSize))*float(gridSize),
                      minimum,maximum);
}
inline HUDRect rtsUIFrameParentRect(const std::vector<RTSUIResolvedFrame>& frames,
                                   const RTSUIFrame& frame,int w,int h){
    if(frame.parent.empty())
        return {0.f,0.f,float(std::max(1,w)),float(std::max(1,h))};
    const auto it=std::find_if(frames.begin(),frames.end(),
        [&](const RTSUIResolvedFrame& item){
            return item.source.id==frame.parent;
        });
    if(it==frames.end())
        throw std::invalid_argument("Missing UI frame parent for alignment");
    return it->bounds;
}
// axis: 0 horizontal, 1 vertical. alignment: 0 start, 1 middle, 2 end.
// Rewrite only the requested axis's offset to preserve the exact authored
// anchor AND the other axis. These are project-pixel values, not DPI pixels.
inline void rtsUIAlignFrame(RTSUIFrame& frame,const HUDRect& parent,
                            int axis,int alignment){
    if(axis!=0 && axis!=1)
        throw std::invalid_argument("UI alignment axis must be 0 or 1");
    if(alignment<0 || alignment>2)
        throw std::invalid_argument("UI alignment must be start/middle/end");
    const float extent=axis==0?parent.w:parent.h;
    const float size=axis==0?frame.width:frame.height;
    const float desired=(extent-size)*float(alignment)*.5f;
    const bool end=axis==0?
        (frame.anchor=="TopRight" || frame.anchor=="BottomRight"):
        (frame.anchor=="BottomLeft" || frame.anchor=="BottomRight");
    const float origin=frame.anchor=="Center"?(extent-size)*.5f:
                       end?(extent-size):0.f;
    const float offset=std::clamp(desired-origin,-2048.f,2048.f);
    if(axis==0)frame.x=offset;
    else frame.y=offset;
}
