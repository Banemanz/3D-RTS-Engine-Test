#pragma once
#include "RTSSkirmish.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <string>

// Pure screen-space layout, command availability, and hit-testing.
// No OpenGL/GLFW dependencies: the renderer and mouse input share ONE layout.
struct HUDRect {
    float x=0,y=0,w=0,h=0;
    bool contains(double px,double py)const {
        return px>=x && py>=y && px<x+w && py<y+h;
    }
};
enum class HUDCommand { None, Minimap, Move, AttackMove, Stop, Build, Ranger,
                        Worker, SelectArmy, SelectWorkers, FocusBase, Patrol, Hold, CancelQueue, Cancel,
                        TrainSlot0, TrainSlot1, TrainSlot2, TrainingPage,
                        CastAbility0, CastAbility1, CastAbility2, AbilityPage };
struct HUDButton {
    HUDCommand command=HUDCommand::None;
    std::string title;
    const char* hotkey="";
    HUDRect bounds{};
    bool enabled=false;
};
class RTSHUDLayout {
public:
    explicit RTSHUDLayout(int w,int h):width(std::max(w,1)),height(std::max(h,1)){
        const float panel=height<620?160.f:194.f;
        topBar={0,22,float(width),46};
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
    int selectionColumns() const {
        return std::max(1,int((selectionArea.w-20.f)/48.f));
    }
    int selectionRows() const {
        return std::max(1,int((selectionArea.h-69.f+4.f)/46.f));
    }
    int selectionCapacity() const {
        return std::min(120,selectionColumns()*selectionRows());
    }
    HUDRect selectionCardBounds(int index) const {
        const int cols=selectionColumns();
        return {selectionArea.x+10.f+(index%cols)*48.f,
                selectionArea.y+69.f+(index/cols)*46.f,44.f,41.f};
    }
    int selectionUnitAt(double x,double y,const RTSSkirmish& game)const {
        if(!selectionArea.contains(x,y))return -1;
        int count=0;
        for(const auto& u:game.units)
            if(u.isAlive() && u.isSelected && u.ownerId==0)++count;
        if(count<=1)return -1; // single selection uses the detailed dossier
        int n=0;
        for(const auto& u:game.units)if(u.isAlive() && u.isSelected && u.ownerId==0){
            if(n>=selectionCapacity())return -1;
            if(selectionCardBounds(n).contains(x,y))return u.id;
            ++n;
        }
        return -1;
    }
    std::array<HUDButton,12> buttons(const RTSSkirmish& game)const {
        int selected=0; bool worker=false,combat=false;
        bool base=false,army=false,workersAvailable=false,selectedBuilding=false;
        int activeSelectedStructure=-1;
        for(const auto& u:game.units)if(u.isAlive() && u.ownerId==0 && u.isSelected){
            ++selected;worker|=u.isWorker;combat|=!u.isStructure;
            if(u.isStructure){selectedBuilding=true;activeSelectedStructure=u.id;}
        }
        const bool barracks=game.completedBarracksCount()>0;
        for(const auto& u:game.units)if(u.ownerId==0 && u.isAlive()){
            army|=!u.isStructure && !u.isWorker;
            workersAvailable|=u.isWorker;
            if(u.id==game.playerBaseId())base=true;
        }
        const bool live=game.result==MatchResult::Playing;
        const bool supply=game.supplyUsed<game.supplyCap;
        const bool producer=game.selectedProducerId()>=0 && game.trainingPageCount()>0;
        const bool paged=producer && game.trainingPageCount()>1;
        const bool abilityPaged=!producer && selected==1 && game.abilityPageCount()>1;
        const std::array<HUDCommand,12> types={
          HUDCommand::Move,HUDCommand::AttackMove,HUDCommand::Stop,
          producer?HUDCommand::TrainSlot0:HUDCommand::Build,
          producer?HUDCommand::TrainSlot1:HUDCommand::Ranger,
          producer?HUDCommand::TrainSlot2:HUDCommand::Worker,
          HUDCommand::SelectArmy,HUDCommand::FocusBase,
          selectedBuilding && !combat?HUDCommand::CancelQueue:HUDCommand::Patrol,
          HUDCommand::Hold,HUDCommand::SelectWorkers,
          paged?HUDCommand::TrainingPage:
          abilityPaged?HUDCommand::AbilityPage:HUDCommand::Cancel
        };
        const std::array<const char*,12> labels={
          "MOVE","ATK MOVE","STOP","BUILD","RANGER","WORKER",
          "ALL ARMY","HOME BASE",selectedBuilding && !combat?"CANCEL Q":"PATROL",
          "HOLD","WORKERS",paged||abilityPaged?"MORE":"CANCEL"
        };
        const std::array<const char*,12> keys={
          "M","A","X","B","T","C","F2","H",
          selectedBuilding && !combat?"V":"P","G","F3",
          paged||abilityPaged?"TAB":"ESC"
        };
        std::array<HUDButton,12> result{};
        const float gap=6;
        const float cw=(commandArea.w-2*gap)/3;
        const float ch=(commandArea.h-3*gap)/4;
        for(int i=0;i<12;++i){
            result[static_cast<size_t>(i)]={
                types[static_cast<size_t>(i)],labels[static_cast<size_t>(i)],
                keys[static_cast<size_t>(i)],
                {commandArea.x+(i%3)*(cw+gap),
                 commandArea.y+(i/3)*(ch+gap),cw,ch},live};
        }
        result[0].enabled=live && combat;
        result[1].enabled=live && combat;
        result[2].enabled=live && combat;
        if(producer) {
            for(int i=0;i<3;++i) {
                const std::string id=game.selectedTrainableId(i);
                const std::string upgrade=game.selectedResearchableId(i);
                std::string title=id.empty()?"EMPTY":id.substr(0,10);
                if(!upgrade.empty()){
                    const auto def=compileUpgrade(game.project().catalog,upgrade);
                    const int level=game.upgradeLevel(0,upgrade);
                    title="TECH "+upgrade.substr(0,7);
                    if(level>=def.maxLevel)title="MAX "+upgrade.substr(0,7);
                    else title+=" "+std::to_string(level+1)+"/"+
                                std::to_string(def.maxLevel);
                }else for(auto& c:title)
                    c=char(std::toupper(static_cast<unsigned char>(c)));
                result[static_cast<size_t>(3+i)].title=title;
                result[static_cast<size_t>(3+i)].enabled=live &&
                    (!upgrade.empty()?game.canResearchSelectedSlot(i):
                                      game.canQueueSelectedTrainingSlot(i));
            }
        }else{
            result[3].enabled=live && worker;
            result[4].enabled=live && barracks && supply;
            result[5].enabled=live && base && supply;
        }
        result[6].enabled=army;
        result[7].enabled=base;
        result[8].enabled=live && (combat ||
            (selectedBuilding && activeSelectedStructure>=0 &&
             game.pendingProduction(activeSelectedStructure)>0));
        result[9].enabled=live && combat;
        result[10].enabled=live && workersAvailable;
        result[11].enabled=true;
        if(paged)result[11].title="PAGE "+std::to_string(game.trainingPage()+1)+"/"+
                                          std::to_string(game.trainingPageCount());
        else if(abilityPaged){
            const int page=game.abilityPage()%game.abilityPageCount();
            result[11].title="SKILLS "+std::to_string(page+1)+"/"+
                             std::to_string(game.abilityPageCount());
        }
        // Single-selection Ability bar reuses the otherwise disabled
        // Build/Train slots when no producer is open. Training remains
        // accessible on production structures, and core orders stay put.
        if(selected==1 && !producer){
            int casterId=-1;
            for(const auto& u:game.units)
                if(u.isSelected && u.ownerId==0 && u.isAlive()){
                    casterId=u.id;break;
                }
            for(int i=0;i<3 && casterId>0;++i){
                const std::string id=game.selectedAbilityId(i);
                if(id.empty())continue;
                const auto fields=game.project().catalog.resolvedFields(id);
                const auto name=fields.find("name");
                const std::string label=name!=fields.end() &&
                    name->second.is<std::string>()?
                    name->second.get<std::string>():id;
                std::string upper=label.substr(0,12);
                for(char& ch:upper)
                    ch=char(std::toupper(static_cast<unsigned char>(ch)));
                HUDButton& button=result[static_cast<std::size_t>(i+3)];
                button.command=i==0?HUDCommand::CastAbility0:
                               i==1?HUDCommand::CastAbility1:HUDCommand::CastAbility2;
                const auto ready=game.abilityCooldownRemaining(casterId,id);
                if(ready>0)
                    upper=upper.substr(0,8)+" "+std::to_string((ready+59)/60)+"S";
                button.title=upper;
                button.hotkey=i==0?"F":i==1?"J":"K";
                button.enabled=live && game.canPrepareAbility(casterId,id);
            }
        }
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
