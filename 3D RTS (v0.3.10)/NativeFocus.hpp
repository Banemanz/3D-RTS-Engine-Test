#pragma once
#include <cstddef>

// Renderer/window-system-agnostic focus navigation. Kept separate from
// individual menus so later native editor panels can share the same rules.
namespace NativeUI {
class FocusList {
    std::size_t selected_=0;
public:
    std::size_t index()const{return selected_;}
    void reset(){selected_=0;}
    bool select(std::size_t index,std::size_t count){
        if(index>=count)return false;
        selected_=index;return true;
    }
    void move(int direction,std::size_t count){
        if(count==0){selected_=0;return;}
        const auto length=static_cast<long long>(count);
        const auto offset=static_cast<long long>(direction)%length;
        const auto next=(static_cast<long long>(selected_%count)+offset+length)%length;
        selected_=static_cast<std::size_t>(next);
    }
};
}
