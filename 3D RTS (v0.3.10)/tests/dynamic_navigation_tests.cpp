#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

static RTSProject isolatedMap(){
    RTSProject project=RTSProject::defaults();
    // Preserve each team's base to prevent auto-victory, but eliminate
    // nonessential threats and mineral blockers from this movement fixture.
    auto& p=project.placements;
    p.erase(std::remove_if(p.begin(),p.end(),[](const ProjectPlacement& item){
        if(item.kind=="Mineral")return true;
        return item.kind=="Unit" && item.catalogId!="command_center" &&
               item.catalogId!="rift_bastion" && item.id!="ranger_1";
    }),p.end());
    project.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    project.validate();
    return project;
}
static Unit& selectRanger(RTSSkirmish& game){
    Unit* chosen=nullptr;
    for(auto& u:game.units){
        u.isSelected=false;
        if(u.catalogId=="ranger"){u.isSelected=true;chosen=&u;}
    }
    assert(chosen);
    return *chosen;
}
static void rerouteAroundNewObstacle(){
    RTSSkirmish game;
    game.loadProject(isolatedMap());
    const int id=selectRanger(game).id;
    const Vec3 destination{-14.f,0.f,30.f};
    assert(game.orderMove(destination,false));
    const Unit& u=selectRanger(game);
    auto initial=game.navigation.findPath(u.position,destination);
    assert(initial.size()>10);
    const auto obstruction=game.navigation.toCell(initial[initial.size()/2]);
    const auto before=game.navigation.revision();
    assert(game.navigation.walkable(obstruction.x,obstruction.z));
    game.navigation.setBlocked(obstruction.x,obstruction.z,true);
    assert(game.navigation.revision()==before+1);
    // A* must avoid the new tile and the sim must notice the revision while
    // the unit is already following its old route.
    const auto newRoute=game.navigation.findPath(u.position,destination);
    assert(!newRoute.empty() && game.navigation.routeClear(u.position,newRoute));
    bool steppedInBlocked=false;
    for(int tick=0;tick<1300 && game.result==MatchResult::Playing;++tick){
        game.update(1.f/60.f);
        const auto& moving=selectRanger(game);
        const auto at=game.navigation.toCell(moving.position);
        if(!game.navigation.walkable(at.x,at.z))steppedInBlocked=true;
    }
    const Unit& arrived=selectRanger(game);
    assert(!steppedInBlocked);
    assert((arrived.position-destination).lengthXZ()<4.f);
}
static void unreachableLegAdvancesQueuedOrders(){
    RTSSkirmish game;
    game.loadProject(isolatedMap());
    Unit& u=selectRanger(game);
    const Vec3 unreachable{-14.f,0.f,32.f};
    const Vec3 fallback{-12.f,0.f,-24.f};
    assert(game.orderMove(unreachable,false));
    assert(game.orderMove(fallback,false,true));
    const int row=game.navigation.toCell({0,0,8.f}).z;
    for(int x=0;x<game.navigation.width();++x)
        game.navigation.setBlocked(x,row,true);
    assert(game.navigation.findPath(u.position,unreachable).empty());
    for(int tick=0;tick<1000 && game.result==MatchResult::Playing;++tick)
        game.update(1.f/60.f);
    const auto& moved=selectRanger(game);
    // The blocked first leg is dropped and the still-reachable queued order
    // is issued, rather than a unit repeatedly colliding with the new wall.
    assert((moved.position-fallback).lengthXZ()<4.f);
}
int main(){
    rerouteAroundNewObstacle();
    unreachableLegAdvancesQueuedOrders();
    std::cout<<"dynamic navigation: rerouting, safe blocked cells and queued fallback passed\n";
}
