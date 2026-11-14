#pragma once
#include <algorithm>
#include <array>
#include <cstddef>

// The native UI's first reusable component is renderer-agnostic.
// Layout and click routing can be unit-tested without OpenGL/GLFW/ImGui.
namespace NativeUI {
struct Rect {
    float x=0.f,y=0.f,w=0.f,h=0.f;
    bool contains(double px,double py)const {
        return px>=x && py>=y && px<x+w && py<y+h;
    }
};
enum class Action { None, Resume, Restart, Editor, Exit };
struct Button { Action action; Rect bounds; const char* text; };

class PauseMenu {
public:
    explicit PauseMenu(int width,int height):
      width_(std::max(1,width)),height_(std::max(1,height)) {
        const float panelW=std::max(1.f,std::min(460.f,float(width_)-24.f));
        const float panelH=std::max(1.f,std::min(380.f,float(height_)-24.f));
        bounds={(float(width_)-panelW)*.5f,(float(height_)-panelH)*.5f,panelW,panelH};
        const float inset=std::min(32.f,panelW*.09f);
        const float top=bounds.y+std::min(98.f,panelH*.26f);
        const float remain=std::max(1.f,bounds.y+panelH-top-24.f);
        const float gap=std::min(12.f,remain*.03f);
        const float itemH=std::max(1.f,(remain-gap*3.f)/4.f);
        constexpr Action actions[]={Action::Resume,Action::Restart,Action::Editor,Action::Exit};
        constexpr const char* labels[]={"RESUME MATCH","RESTART MATCH","OPEN WORLD EDITOR","EXIT GAME"};
        for(int i=0;i<4;++i)buttons_[static_cast<std::size_t>(i)]={
            actions[i],{bounds.x+inset,top+i*(itemH+gap),
                        std::max(1.f,panelW-inset*2.f),itemH},labels[i]};
    }
    int width()const{return width_;}
    int height()const{return height_;}
    Rect bounds{};
    const std::array<Button,4>& buttons()const{return buttons_;}
    Action hit(double x,double y)const{
        for(const auto& b:buttons_)if(b.bounds.contains(x,y))return b.action;
        return Action::None;
    }
private:
    int width_=1,height_=1;
    std::array<Button,4> buttons_{};
};
}
