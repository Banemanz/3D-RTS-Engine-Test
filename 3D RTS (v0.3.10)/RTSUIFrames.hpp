#pragma once
#include "RTSProject.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Original authorable in-game frame system. The project schema is bounded and
// declarative: NO interpreted scripts, unsafe content, raw graphics pointers,
// or side effects during loading. Values are never simulation commands.
struct RTSUIFrame {
    std::string id;
    std::string parent;      // nonempty -> ID of an EARLIER frame
    std::string kind="Panel"; // Panel, Label, Button, Bar
    std::string anchor="TopLeft"; // TopLeft, TopRight, BottomLeft, BottomRight, Center
    std::string palette="Navy";   // Navy, Teal, Cobalt, Amber, Slate
    std::string text;
    std::string binding="None";   // None, Minerals, Supply, Workers, Selected,
                                   // SelectedHealth, Message, Ticks
    std::string action="None";    // None, SelectArmy, SelectWorkers, FocusBase, Trigger
    // F6 variable-driven interface. Pure live presentation: never writes
    // the trigger variable or creates unrecorded game-side mutations.
    std::string variable;           // required when binding="Variable"
    int barMin=0, barMax=100;       // integer range for Variable progress bars
    std::string visibleVariable;    // nonempty => visibility if value >= threshold
    int visibleAtLeast=1;
    float x=20.f, y=90.f, width=200.f, height=50.f;
    bool consumeInput=false;      // panel or button optionally blocks world clicks
};
inline bool rtsUIFrameChoice(const std::string& choice,
                            const char* const* values,std::size_t count){
    for(std::size_t i=0;i<count;++i)if(choice==values[i])return true;
    return false;
}
inline void rtsValidateUIFrames(const std::vector<RTSUIFrame>& frames){
    if(frames.size()>32)throw std::invalid_argument("ui_frames: maximum 32 frames");
    const char* kinds[]={"Panel","Label","Button","Bar"};
    const char* anchors[]={"TopLeft","TopRight","BottomLeft","BottomRight","Center"};
    const char* palettes[]={"Navy","Teal","Cobalt","Amber","Slate"};
    const char* bindings[]={"None","Minerals","Supply","Workers",
                             "Selected","SelectedHealth","Message","Ticks","Variable"};
    const char* actions[]={"None","SelectArmy","SelectWorkers","FocusBase","Trigger"};
    std::set<std::string> previous;
    for(const auto& f:frames){
        if(f.id.empty()||f.id.size()>48)
            throw std::invalid_argument("ui_frames: frame ID must have 1..48 characters");
        for(const unsigned char ch:f.id)
            if(!(ch>='a'&&ch<='z') && !(ch>='A'&&ch<='Z') &&
               !(ch>='0'&&ch<='9') && ch!='_' && ch!='-')
                throw std::invalid_argument("ui_frames: invalid frame ID: "+f.id);
        if(!previous.insert(f.id).second)
            throw std::invalid_argument("ui_frames: duplicate frame ID: "+f.id);
        int depth=0;
        if(!f.parent.empty()){
            auto it=previous.find(f.parent);
            if(it==previous.end() || f.parent==f.id)
                throw std::invalid_argument("ui_frames: parent must precede child: "+f.id);
            // The vector order is a topological parent-first order. Counting
            // parent ancestry is bounded and guards against accidental depth.
            std::string parent=f.parent;
            while(!parent.empty()){
                ++depth;
                if(depth>6)throw std::invalid_argument("ui_frames: maximum nesting depth is 6");
                auto found=std::find_if(frames.begin(),frames.end(),
                    [&](const RTSUIFrame& e){return e.id==parent;});
                if(found==frames.end() || found->kind!="Panel")
                    throw std::invalid_argument("ui_frames: only Panels can be parents");
                parent=found->parent;
            }
        }
        if(!rtsUIFrameChoice(f.kind,kinds,4) ||
           !rtsUIFrameChoice(f.anchor,anchors,5) ||
           !rtsUIFrameChoice(f.palette,palettes,5) ||
           !rtsUIFrameChoice(f.binding,bindings,9) ||
           !rtsUIFrameChoice(f.action,actions,5))
            throw std::invalid_argument("ui_frames: unknown kind/anchor/palette/binding/action: "+f.id);
        if(f.kind!="Button" && f.action!="None")
            throw std::invalid_argument("ui_frames: only Buttons may bind actions: "+f.id);
        if(f.action=="Trigger" && !f.consumeInput)
            throw std::invalid_argument("ui_frames: scripted Button must consume world clicks: "+f.id);
        if(f.kind=="Bar" &&
           f.binding!="Supply" && f.binding!="SelectedHealth" &&
           f.binding!="Variable")
            throw std::invalid_argument("ui_frames: Bar binding must be Supply, SelectedHealth or Variable");
        const auto isVarId=[](const std::string& name)->bool{
            if(name.empty() || name.size()>64 ||
               !((name.front()>='A' && name.front()<='Z') ||
                 (name.front()>='a' && name.front()<='z') ||
                 name.front()=='_'))return false;
            for(const char c:name)
                if(!((c>='A' && c<='Z') || (c>='a' && c<='z') ||
                     (c>='0' && c<='9') || c=='_'))return false;
            return true;
        };
        if(f.binding=="Variable"){
            if(!isVarId(f.variable))
                throw std::invalid_argument("ui_frames: Variable binding needs a valid F6 variable: "+f.id);
        }else if(!f.variable.empty())
            throw std::invalid_argument("ui_frames: unused Variable reference: "+f.id);
        if(!f.visibleVariable.empty() && !isVarId(f.visibleVariable))
            throw std::invalid_argument("ui_frames: invalid visibility variable: "+f.id);
        if(f.barMin<-100000000 || f.barMin>100000000 ||
           f.barMax<-100000000 || f.barMax>100000000 ||
           f.visibleAtLeast<-100000000 || f.visibleAtLeast>100000000)
            throw std::invalid_argument("ui_frames: invalid variable range/threshold: "+f.id);
        if(f.kind=="Bar" && f.binding=="Variable" && f.barMin>=f.barMax)
            throw std::invalid_argument("ui_frames: Variable bar needs max > min: "+f.id);
        if(f.text.size()>128)
            throw std::invalid_argument("ui_frames: text limit is 128 ASCII characters");
        for(unsigned char ch:f.text)
            if(ch<32 || ch>126)
                throw std::invalid_argument("ui_frames: renderer currently supports printable ASCII text");
        auto number=[&](float n,float lo,float hi,const char* field){
            if(!std::isfinite(n)||n<lo||n>hi)
                throw std::invalid_argument("ui_frames: "+f.id+" invalid "+field);
        };
        number(f.x,-2048.f,2048.f,"x");
        number(f.y,-2048.f,2048.f,"y");
        number(f.width,24.f,650.f,"width");
        number(f.height,20.f,400.f,"height");
    }
}
// Can be used by the editor independently of an RTSProject; validation is
// intentionally the same for authoring and packaged gameplay.
inline picojson::value rtsUIFramesToJson(const std::vector<RTSUIFrame>& frames){
    rtsValidateUIFrames(frames);
    picojson::array output;
    for(const auto& f:frames){
        picojson::object v;
        v["id"]=picojson::value(f.id);
        v["parent"]=picojson::value(f.parent);
        v["kind"]=picojson::value(f.kind);
        v["anchor"]=picojson::value(f.anchor);
        v["palette"]=picojson::value(f.palette);
        v["text"]=picojson::value(f.text);
        v["binding"]=picojson::value(f.binding);
        v["action"]=picojson::value(f.action);
        v["x"]=picojson::value(double(f.x));
        v["y"]=picojson::value(double(f.y));
        v["width"]=picojson::value(double(f.width));
        v["height"]=picojson::value(double(f.height));
        v["consume_input"]=picojson::value(f.consumeInput);
        if(f.binding=="Variable")v["variable"]=picojson::value(f.variable);
        if(f.barMin!=0)v["bar_min"]=picojson::value(double(f.barMin));
        if(f.barMax!=100)v["bar_max"]=picojson::value(double(f.barMax));
        if(!f.visibleVariable.empty()){
            v["visible_variable"]=picojson::value(f.visibleVariable);
            v["visible_at_least"]=picojson::value(double(f.visibleAtLeast));
        }
        output.emplace_back(v);
    }
    return picojson::value(output);
}
inline std::vector<RTSUIFrame> rtsUIFramesFromProject(const RTSProject& project){
    const auto it=project.metadata.find("ui_frames");
    if(it==project.metadata.end())return {};
    if(!it->second.is<picojson::array>())
        throw std::invalid_argument("ui_frames must be an array");
    const auto& arr=it->second.get<picojson::array>();
    if(arr.size()>32)throw std::invalid_argument("ui_frames: maximum 32 frames");
    std::vector<RTSUIFrame> result;
    result.reserve(arr.size());
    for(const auto& item:arr){
        if(!item.is<picojson::object>())
            throw std::invalid_argument("ui_frames: each entry must be an object");
        const auto& o=item.get<picojson::object>();
        static const std::set<std::string> allowed={
            "id","parent","kind","anchor","palette","text",
            "binding","action","x","y","width","height","consume_input",
            "variable","bar_min","bar_max","visible_variable","visible_at_least"};
        for(const auto& kv:o)
            if(!allowed.count(kv.first))
                throw std::invalid_argument("ui_frames: unknown property "+kv.first);
        RTSUIFrame f;
        const auto str=[&](const char* name,std::string& value){
            const auto k=o.find(name);
            if(k==o.end())return;
            if(!k->second.is<std::string>())
                throw std::invalid_argument(std::string("ui_frames: ")+name+" must be text");
            value=k->second.get<std::string>();
        };
        const auto num=[&](const char* name,float& value){
            const auto k=o.find(name);
            if(k==o.end())return;
            if(!k->second.is<double>())
                throw std::invalid_argument(std::string("ui_frames: ")+name+" must be numeric");
            const double number=k->second.get<double>();
            if(!std::isfinite(number) || number<-100000. || number>100000.)
                throw std::invalid_argument(std::string("ui_frames: invalid ")+name);
            value=static_cast<float>(number);
        };
        str("id",f.id);str("parent",f.parent);str("kind",f.kind);
        str("anchor",f.anchor);str("palette",f.palette);
        str("text",f.text);str("binding",f.binding);str("action",f.action);
        str("variable",f.variable);
        str("visible_variable",f.visibleVariable);
        const auto integer=[&](const char* name,int& value){
            auto found=o.find(name);
            if(found==o.end())return;
            if(!found->second.is<double>())
                throw std::invalid_argument(std::string("ui_frames: ")+name+" must be integer");
            const double n=found->second.get<double>();
            if(!std::isfinite(n) || n!=std::floor(n) ||
               n<-100000000. || n>100000000.)
                throw std::invalid_argument(std::string("ui_frames: ")+name+" out of range");
            value=static_cast<int>(n);
        };
        integer("bar_min",f.barMin);integer("bar_max",f.barMax);
        integer("visible_at_least",f.visibleAtLeast);
        num("x",f.x);num("y",f.y);num("width",f.width);num("height",f.height);
        const auto consume=o.find("consume_input");
        if(consume!=o.end()){
            if(!consume->second.is<bool>())
                throw std::invalid_argument("ui_frames: consume_input must be boolean");
            f.consumeInput=consume->second.get<bool>();
        }else f.consumeInput=f.kind=="Panel" || f.kind=="Button";
        result.push_back(std::move(f));
    }
    rtsValidateUIFrames(result);
    return result;
}
