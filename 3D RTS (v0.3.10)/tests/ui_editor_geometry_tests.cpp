#include "RTSUIEditorGeometry.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "UI geometry editor tests require assertions in Release"
#endif
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
bool close(float a,float b){return std::fabs(a-b)<.0001f;}
bool throwsGrid(int grid){
    try{(void)rtsUISnapCoordinate(25.f,grid,true);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(){
    assert(close(rtsUISnapCoordinate(13.f,8,true),16.f));
    assert(close(rtsUISnapCoordinate(-13.f,8,true),-16.f));
    assert(close(rtsUISnapCoordinate(3.f,8,true),0.f));
    assert(close(rtsUISnapCoordinate(13.25f,8,false),13.25f));
    assert(close(rtsUISnapCoordinate(2060.f,16,true),2048.f));
    assert(close(rtsUISnapCoordinate(-2060.f,16,true),-2048.f));
    assert(close(rtsUISnapDimension(50.f,8,true,24.f,650.f),48.f));
    assert(close(rtsUISnapDimension(20.f,8,true,20.f,400.f),24.f));
    assert(close(rtsUISnapDimension(660.f,32,true,24.f,650.f),640.f));
    assert(close(rtsUISnapDimension(400.f,32,false,24.f,400.f),400.f));
    assert(throwsGrid(5) && throwsGrid(-4));
    assert(close(rtsUISnapCoordinate(std::nanf(""),8,true),0.f));
    assert(close(rtsUISnapDimension(std::nanf(""),8,true,20.f,400.f),20.f));

    RTSUIFrame panel;
    panel.id="panel";panel.kind="Panel";panel.anchor="TopRight";
    panel.x=-25.f;panel.y=120.f;panel.width=304.f;panel.height=240.f;
    RTSUIFrame child;
    child.id="child";child.kind="Button";child.parent="panel";
    child.anchor="BottomRight";child.x=-12.f;child.y=-10.f;
    child.width=120.f;child.height=45.f;child.action="SelectArmy";
    const std::vector<RTSUIFrame> sample{panel,child};
    const auto layout=rtsResolveUIFrames(sample,1280,720);
    assert(layout.size()==2);
    const auto root=rtsUIFrameParentRect(layout,panel,1280,720);
    const auto parent=rtsUIFrameParentRect(layout,child,1280,720);
    assert(close(root.w,1280.f) && close(root.h,720.f));
    assert(close(parent.w,304.f) && close(parent.h,240.f));
    assert(close(parent.x,951.f));

    for(const auto wh:std::vector<std::pair<int,int>>{
            {640,480},{800,600},{1280,720},{1920,1080}}){
        const HUDRect screen{0,0,float(wh.first),float(wh.second)};
        // Relative anchor must be preserved even when applying absolute
        // alignment commands. Other axis must remain untouched.
        for(int where=0;where<3;++where){
            auto edit=panel;
            const float oldY=edit.y;
            rtsUIAlignFrame(edit,screen,0,where);
            assert(edit.anchor=="TopRight" && close(edit.y,oldY));
            const auto calc=rtsResolveUIFrames({edit},wh.first,wh.second);
            const float expected=(float(wh.first)-edit.width)*float(where)/2.f;
            assert(close(calc.front().bounds.x,expected));
            rtsUIAlignFrame(edit,screen,1,where);
            const auto vertical=rtsResolveUIFrames({edit},wh.first,wh.second);
            assert(close(vertical.front().bounds.y,
                (float(wh.second)-edit.height)*float(where)/2.f));
        }
    }
    auto centered=child;
    rtsUIAlignFrame(centered,parent,0,1);
    rtsUIAlignFrame(centered,parent,1,1);
    const auto nested=rtsResolveUIFrames({panel,centered},1280,720);
    assert(close(nested[1].bounds.x,
        nested[0].bounds.x+(nested[0].bounds.w-centered.width)/2.f));
    assert(close(nested[1].bounds.y,
        nested[0].bounds.y+(nested[0].bounds.h-centered.height)/2.f));
    assert(centered.anchor=="BottomRight");
    auto bad=panel;
    bool failed=false;
    try{rtsUIAlignFrame(bad,root,3,0);}
    catch(const std::invalid_argument&){failed=true;}
    assert(failed);
    failed=false;
    try{rtsUIAlignFrame(bad,root,0,4);}
    catch(const std::invalid_argument&){failed=true;}
    assert(failed);
    failed=false;
    RTSUIFrame orphan=child;
    orphan.parent="missing";
    try{(void)rtsUIFrameParentRect(layout,orphan,1280,720);}
    catch(const std::invalid_argument&){failed=true;}
    assert(failed);
    std::cout<<"UI editor placement: repeatable pixel-grid snap, bounded resize, "
                "parent-relative six-way alignment, 4 resolutions and "
                "DPI-independent geometry passed\n";
}
