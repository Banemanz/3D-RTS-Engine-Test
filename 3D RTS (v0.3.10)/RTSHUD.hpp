#pragma once
#include "RTSHUDLayout.hpp"
#include "RTSCamera.hpp"
#include "BitmapHUD.hpp"
#include <algorithm>
#include <cmath>
#include <string>

// Render-only HUD: all hit regions come from RTSHUDLayout and are shared with input.
// Procedural placeholders for icons until model/texture assets are available.
class RTSHUD {
    GLuint shader_,vao_,vbo_;
    const RTSHUDLayout& ui_;
    int w_,h_;
    void rect(float x,float y,float width,float height,float r,float g,float b,float a=1.f)const {
        if(width<=0||height<=0)return;
        const float x0=2.f*x/w_-1.f,x1=2.f*(x+width)/w_-1.f;
        const float y0=1.f-2.f*y/h_,y1=1.f-2.f*(y+height)/h_;
        const float data[]={x0,y0,x1,y0,x1,y1,x0,y0,x1,y1,x0,y1};
        glUseProgram(shader_);
        glUniform4f(glGetUniformLocation(shader_,"uColor"),r,g,b,a);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES,0,6);
    }
    void text(float x,float y,float size,const std::string& s,
              float r=.87f,float g=.91f,float b=.95f)const {
        drawHUDText(shader_,vao_,vbo_,w_,h_,x,y,size,s,r,g,b);
    }
    static std::string shortText(std::string s,size_t max){
        if(s.size()>max)s=s.substr(0,max>3?max-3:0)+"...";
        return s;
    }
public:
    RTSHUD(GLuint shader,GLuint vao,GLuint vbo,const RTSHUDLayout& layout):
        shader_(shader),vao_(vao),vbo_(vbo),ui_(layout),w_(layout.width),h_(layout.height){}
    void draw(const RTSSkirmish& game,const RTSProject& project,
              const RTSCamera& camera,int commandMode,const std::string& notice)const {
        const auto& top=ui_.topBar;
        const auto& bottom=ui_.bottom;
        rect(top.x,top.y,top.w,top.h,.045f,.073f,.105f,.96f);
        rect(0,top.h-3,float(w_),3,.10f,.56f,.61f);
        text(16,11,2.5f,"FRONTIER COMMAND",.72f,.92f,.94f);
        const float anchor=std::max(235.f,float(w_)-565.f);
        text(anchor,9,1.8f,"MINERALS",.53f,.71f,.75f);
        text(anchor,24,2.2f,std::to_string(game.playerMinerals),.94f,.78f,.30f);
        text(anchor+140,9,1.8f,"SUPPLY",.53f,.71f,.75f);
        text(anchor+140,24,2.2f,std::to_string(game.supplyUsed)+"/"+
             std::to_string(game.supplyCap),.88f,.94f,.95f);
        text(anchor+270,9,1.8f,"WORKERS",.53f,.71f,.75f);
        text(anchor+270,24,2.2f,std::to_string(game.activeWorkerCount()),.84f,.94f,.83f);
        text(anchor+402,9,1.8f,"ARMY",.53f,.71f,.75f);
        int army=0;
        for(const auto& u:game.units)if(u.isAlive() && u.ownerId==0 &&
                                        !u.isWorker && !u.isStructure)++army;
        text(anchor+402,24,2.2f,std::to_string(army),.84f,.94f,.83f);

        rect(bottom.x,bottom.y,bottom.w,bottom.h,.035f,.055f,.079f,.985f);
        rect(0,bottom.y,float(w_),3,.18f,.41f,.47f);
        const HUDRect mm=ui_.minimap;
        rect(mm.x-5,mm.y-5,mm.w+10,mm.h+10,.10f,.17f,.21f);
        rect(mm.x,mm.y,mm.w,mm.h,.065f,.16f,.17f);
        // 12x12 terrain material overview. This draws only 144 fixed tiles,
        // regardless of map resolution or game unit count.
        constexpr int tiles=12;
        for(int z=0;z<tiles;++z)for(int x=0;x<tiles;++x){
            int cx=x*(project.terrain.width()-1)/(tiles-1);
            int cz=z*(project.terrain.depth()-1)/(tiles-1);
            const auto& cell=project.terrain.cell(cx,cz);
            float r=.12f,g=.27f,b=.23f;
            if(cell.material==1){r=.31f;g=.25f;b=.18f;}
            else if(cell.material==2){r=.35f;g=.38f;b=.40f;}
            else if(cell.material==3){r=.53f;g=.48f;b=.29f;}
            else if(cell.material==5){r=.09f;g=.25f;b=.34f;}
            if(cell.blocked){r*=.55f;g*=.55f;b*=.55f;}
            const auto vis=game.vision.cell(cx,cz);
            if(vis==RTSVision::State::Unexplored){r=.012f;g=.018f;b=.023f;}
            else if(vis==RTSVision::State::Explored){r*=.42f;g*=.42f;b*=.42f;}
            const float tileW=mm.w/tiles,tileH=mm.h/tiles;
            rect(mm.x+x*tileW,mm.y+z*tileH,tileW+.25f,tileH+.25f,r,g,b);
        }
        auto dot=[&](const Vec3& world,float radius,float r,float g,float b){
            const float rangeX=(project.terrain.width()-1)*project.terrain.spacing();
            const float rangeZ=(project.terrain.depth()-1)*project.terrain.spacing();
            const float x=mm.x+std::clamp((world.x-project.terrain.minX())/rangeX,0.f,1.f)*mm.w;
            const float y=mm.y+std::clamp((world.z-project.terrain.minZ())/rangeZ,0.f,1.f)*mm.h;
            rect(x-radius,y-radius,2*radius,2*radius,r,g,b);
        };
        for(const auto& mineral:game.minerals)
            if(mineral.remaining>0 && game.vision.explored(mineral.position))
                dot(mineral.position,2.f,.20f,.88f,1.f);
        for(const auto& unit:game.units)if(unit.isAlive() &&
            (unit.ownerId==0 || game.vision.visible(unit.position)))
            dot(unit.position,unit.isStructure?3.5f:2.0f,
                unit.ownerId==0?.26f:.98f,unit.ownerId==0?.87f:.32f,
                unit.ownerId==0?.75f:.31f);
        dot(camera.focalPoint,2.5f,1.f,1.f,.96f);
        text(mm.x+6,mm.y+5,1.6f,"TACTICAL MAP",.95f,.96f,.87f);
        rect(mm.x+2,mm.y+mm.h-17,mm.w-4,15,.04f,.08f,.11f,.85f);
        text(mm.x+8,mm.y+mm.h-13,1.4f,"LMB PAN",.69f,.88f,.92f);

        const auto s=ui_.selectionArea;
        rect(s.x,s.y,s.w,s.h,.075f,.105f,.139f,.85f);
        rect(s.x,s.y,s.w,3,.23f,.51f,.56f);
        text(s.x+12,s.y+12,2.0f,"SELECTION",.62f,.81f,.84f);
        int count=0;const Unit* first=nullptr;float totalHp=0,totalMax=0;
        bool allSelectedHolding=true;
        for(const auto& u:game.units)if(u.isAlive() && u.isSelected && u.ownerId==0){
            ++count;if(!first)first=&u;totalHp+=u.currentHp;totalMax+=u.data.max_hp;
            allSelectedHolding=allSelectedHolding && u.holdPositionEnabled;
        }
        if(first){
            text(s.x+12,s.y+43,2.5f,shortText(count==1?first->instanceName:
                std::to_string(count)+" UNITS SELECTED",
                static_cast<size_t>(std::max(12.f,(s.w-24)/15.f))),
                1.f,.88f,.55f);
            if(count>1) {
                int n=0;
                const int capacity=ui_.selectionCapacity();
                for(const auto& unit:game.units)if(unit.isAlive() && unit.isSelected &&
                                                   unit.ownerId==0){
                    if(n>=capacity)break;
                    const HUDRect b=ui_.selectionCardBounds(n++);
                    rect(b.x,b.y,b.w,b.h,.12f,.17f,.21f,.97f);
                    if(unit.holdPositionEnabled)
                        rect(b.x,b.y,b.w,3,.94f,.73f,.26f,.98f);
                    rect(b.x+2,b.y+2,b.w-4,b.h-12,
                         unit.data.faction_color.x*.67f,
                         unit.data.faction_color.y*.67f,
                         unit.data.faction_color.z*.67f,.93f);
                    std::string abbrev=unit.isWorker?"WK":unit.isStructure?"HQ":
                                       unit.catalogId.substr(0,2);
                    text(b.x+7,b.y+10,2.f,abbrev,1.f,1.f,1.f);
                    const float health=std::clamp(unit.currentHp/
                         std::max(.01f,unit.data.max_hp),0.f,1.f);
                    rect(b.x+3,b.y+b.h-8,b.w-6,5,.08f,.12f,.15f);
                    rect(b.x+3,b.y+b.h-8,(b.w-6)*health,5,
                         health>.4f?.22f:.9f,health>.4f?.86f:.46f,.35f);
                }
                if(count>capacity)
                    text(s.x+12,s.y+s.h-15,1.5f,
                         "+"+std::to_string(count-capacity)+" MORE UNITS",
                         .81f,.80f,.53f);
            }else{
            text(s.x+12,s.y+75,1.9f,
                 "HP  "+std::to_string(int(totalHp))+" / "+std::to_string(int(totalMax)),
                 .8f,.89f,.91f);
            const float hp=totalMax>0?std::clamp(totalHp/totalMax,0.f,1.f):0.f;
            rect(s.x+12,s.y+102,s.w-24,11,.11f,.16f,.19f);
            rect(s.x+12,s.y+102,(s.w-24)*hp,11,
                 hp>.45f?.24f:.87f,hp>.45f?.85f:.48f,.43f);
            if(count==1) {
                if(first->isStructure) {
                    text(s.x+12,s.y+std::min(130.f,s.h-29.f),1.8f,
                        "QUEUE "+std::to_string(game.pendingProduction(first->id))+
                        "  RMB SET RALLY",.61f,.8f,.87f);
                    const float progress=game.productionProgress(first->id);
                    if(progress>=0.f){
                        rect(s.x+12,s.y+s.h-13,s.w-24,6,.13f,.20f,.24f);
                        rect(s.x+12,s.y+s.h-13,(s.w-24)*progress,6,
                             .32f,.77f,.91f);
                    }
                } else {
                    text(s.x+12,s.y+130,1.8f,
                         std::string(first->isWorker?"WORKER  ":"COMBAT  ")+
                         "DMG "+std::to_string(int(first->data.damage))+
                         "  ARM "+std::to_string(int(first->data.armor))+
                         (first->holdPositionEnabled?"  HOLD":""),.61f,.8f,.87f);
                }
            }
            } // one selected unit
        }else {
            text(s.x+12,s.y+50,2.1f,"NO UNITS SELECTED",.84f,.86f,.89f);
            text(s.x+12,s.y+84,1.7f,"SELECT UNITS OR PRESS F2",.56f,.78f,.83f);
        }
        const auto buttons=ui_.buttons(game);
        for(const auto& button:buttons){
            const HUDRect b=button.bounds;
            const bool active=(commandMode==1 && button.command==HUDCommand::Move) ||
                (commandMode==2 && button.command==HUDCommand::AttackMove) ||
                (commandMode==3 && button.command==HUDCommand::Build) ||
                (commandMode==4 && button.command==HUDCommand::Patrol) ||
                (button.command==HUDCommand::Hold && count>0 && allSelectedHolding);
            rect(b.x,b.y,b.w,b.h,button.enabled?.095f:.065f,
                 button.enabled?.17f:.09f,button.enabled?.20f:.11f,.95f);
            rect(b.x,b.y,b.w,2,active?.88f:button.enabled?.24f:.13f,
                 active?.63f:button.enabled?.59f:.19f,
                 active?.27f:button.enabled?.62f:.22f);
            const bool compact=b.h<33.f;
            const float titleSize=compact?1.45f:
                                  button.command==HUDCommand::AttackMove?1.60f:1.70f;
            text(b.x+7,b.y+(compact?6.f:7.f),titleSize,
                 button.title,button.enabled?.88f:.40f,
                 button.enabled?.92f:.47f,button.enabled?.92f:.50f);
            if(!compact)
                text(b.x+7,b.y+b.h-13,1.35f,button.hotkey,.61f,.78f,.81f);
        }
        if(!notice.empty() || !game.triggerMessage.empty()){
            std::string message=notice.empty()?game.triggerMessage:notice;
            rect(float(w_)*.23f,55,float(w_)*.54f,36,.045f,.080f,.105f,.91f);
            text(float(w_)*.23f+12,66,2.f,shortText(message,55),1.f,.79f,.39f);
        }
        if(game.result!=MatchResult::Playing){
            rect(float(w_)*.24f,float(h_)*.40f,float(w_)*.52f,72,.04f,.07f,.11f,.97f);
            text(float(w_)*.29f,float(h_)*.40f+18,4.f,
                 game.result==MatchResult::Victory?"VICTORY":"DEFEAT",1.f,.79f,.31f);
            text(float(w_)*.29f,float(h_)*.40f+53,1.8f,"PRESS R TO RESTART",.84f,.91f,.94f);
        }
    }
};
