#pragma once
#include "RTSSkirmish.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// Pure screen-space layout, command availability, and hit-testing.
// No OpenGL/GLFW dependencies: the renderer and mouse input share ONE layout.
struct HUDRect {
    float x=0,y=0,w=0,h=0;
    bool contains(double px,double py)const {
        return px>=x && py>=y && px<x+w && py<y+h;
    }
};
enum class HUDCommand { None, Minimap, Move, AttackMove, Stop, Build, Marine,
                        Worker, SelectArmy, FocusBase, Cancel };
struct HUDButton {
    HUDCommand command=HUDCommand::None;
    const char* title="";
    const char* hotkey="";
    HUDRect bounds{};
    bool enabled=false;
};
class RTSHUDLayout {
public:
    explicit RTSHUDLayout(int w,int h):width(std::max(w,1)),height(std::max(h,1)){
        const float panel=height<620?160.f:194.f;
        topBar={0,0,float(width),46};
        bottom={0,float(height)-panel,float(width),panel};
        minimap={14,bottom.y+22,panel-37,panel-40};
        commandArea={float(width)-344,bottom.y+20,330,panel-34};
        selectionArea={minimap.x+minimap.w+24,bottom.y+20,
                       std::max(40.f,commandArea.x-(minimap.x+minimap.w+34)),panel-34};
    }
    int width=0,height=0;
    HUDRect topBar,bottom,minimap,selectionArea,commandArea;
    bool consumes(double x,double y)const {
        return topBar.contains(x,y) || bottom.contains(x,y);
    }
    std::array<HUDButton,9> buttons(const RTSSkirmish& game)const {
        int selected=0; bool worker=false,combat=false;
        for(const auto& u:game.units)if(u.isAlive() && u.ownerId==0 && u.isSelected){
            ++selected;worker|=u.isWorker;combat|=!u.isStructure;
        }
        bool base=false,barracks=false,army=false;
        for(const auto& u:game.units)if(u.ownerId==0 && u.isAlive()){
            army|=!u.isStructure && !u.isWorker;
            if(u.catalogId=="command_center")base=true;
            if(u.catalogId=="barracks")barracks=true;
        }
        const bool live=game.result==MatchResult::Playing;
        const bool supply=game.supplyUsed<game.supplyCap;
        const std::array<HUDCommand,9> types={
          HUDCommand::Move,HUDCommand::AttackMove,HUDCommand::Stop,
          HUDCommand::Build,HUDCommand::Marine,HUDCommand::Worker,
          HUDCommand::SelectArmy,HUDCommand::FocusBase,HUDCommand::Cancel
        };
        const std::array<const char*,9> labels={
          "MOVE","ATTACK-MOVE","STOP","BUILD","MARINE","WORKER",
          "ALL ARMY","HOME BASE","CANCEL"
        };
        const std::array<const char*,9> keys={"M","A","X","B","T","C","F2","H","ESC"};
        std::array<HUDButton,9> result{};
        const float gap=6;
        const float cw=(commandArea.w-2*gap)/3;
        const float ch=(commandArea.h-2*gap)/3;
        for(int i=0;i<9;++i){
            result[static_cast<size_t>(i)]={
                types[static_cast<size_t>(i)],labels[static_cast<size_t>(i)],
                keys[static_cast<size_t>(i)],
                {commandArea.x+(i%3)*(cw+gap),
                 commandArea.y+(i/3)*(ch+gap),cw,ch},live};
        }
        result[0].enabled=live && combat;
        result[1].enabled=live && combat;
        result[2].enabled=live && combat;
        result[3].enabled=live && worker;
        result[4].enabled=live && barracks && supply;
        result[5].enabled=live && base && supply;
        result[6].enabled=army;
        result[7].enabled=base;
        result[8].enabled=true;
        return result;
    }
    HUDCommand hit(double x,double y,const RTSSkirmish& game)const {
        if(!consumes(x,y))return HUDCommand::None;
        if(minimap.contains(x,y))return HUDCommand::Minimap;
        for(const auto& button:buttons(game))
            if(button.bounds.contains(x,y))
                return button.enabled?button.command:HUDCommand::Cancel;
        return HUDCommand::Cancel; // consume the rest of the HUD, never issue world orders
    }
    Vec3 minimapWorld(double x,double y,const TerrainMap& terrain)const {
        const float u=std::clamp(float((x-minimap.x)/minimap.w),0.f,1.f);
        const float v=std::clamp(float((y-minimap.y)/minimap.h),0.f,1.f);
        const float px=terrain.minX()+u*(terrain.width()-1)*terrain.spacing();
        const float pz=terrain.minZ()+v*(terrain.depth()-1)*terrain.spacing();
        return terrain.clampToMap(Vec3(px,0,pz));
    }
};
