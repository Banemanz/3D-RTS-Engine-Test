#include "NativePauseMenu.hpp"
#include <cassert>
#include <iostream>
#include <utility>
int main(){
    using namespace NativeUI;
    constexpr std::pair<int,int> resolutions[]={{1280,720},{640,480},{320,240},{1920,1080},{1,1}};
    constexpr Action actions[]={Action::Resume,Action::Restart,Action::Editor,Action::Exit};
    for(const auto& dims:resolutions){
        const PauseMenu m(dims.first,dims.second);
        assert(m.bounds.x>=0.f && m.bounds.y>=0.f);
        assert(m.bounds.x+m.bounds.w<=m.width()+.001f);
        assert(m.bounds.y+m.bounds.h<=m.height()+.001f);
        for(std::size_t i=0;i<m.buttons().size();++i){
            const auto& b=m.buttons()[i];
            assert(b.action==actions[i] && b.text!=nullptr);
            assert(m.hit(b.bounds.x+b.bounds.w*.5f,
                         b.bounds.y+b.bounds.h*.5f)==actions[i]);
        }
        assert(m.hit(-10,-10)==Action::None);
    }
    std::cout<<"native UI layout and hit tests passed\n";
}
