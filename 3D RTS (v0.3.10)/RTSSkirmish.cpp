#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace {
float distanceXZ(const Vec3& a,const Vec3& b) {
    return (a-b).lengthXZ();
}
UnitData marine() {
    UnitData d=UnitData::fromTresString(HUMAN_MARINE_TRES);
    return d;
}
UnitData zealot() {
    UnitData d=UnitData::fromTresString(HYBRID_ZEALOT_TRES);
    return d;
}
UnitData workerData() {
    UnitData d=marine();
    d.resourceName="Human Worker";d.max_hp=40;d.damage=2;d.attack_range=1.5f;
    d.attack_cooldown=1.3f;d.move_speed=7;d.visual_scale=.72f;
    d.mineral_cost=40;d.supply_cost=1;return d;
}
UnitData baseData(bool human) {
    UnitData d=human?marine():zealot();
    d.resourceName=human?"Command Center":"Hive";
    d.max_hp=700;d.armor=2;d.damage=0;d.attack_range=0;d.aggro_range=0;
    d.visual_scale=2.8f;d.supply_cost=0;return d;
}
UnitData barracksData() {
    UnitData d=baseData(true);
    d.resourceName="Barracks";d.max_hp=350;d.visual_scale=2.0f;
    return d;
}
}
RTSSkirmish::RTSSkirmish(){reset();}
Unit& RTSSkirmish::spawn(const UnitData& d,const Vec3& at,int owner,
                       const std::string& name,bool structure,bool worker) {
    units.emplace_back(nextId_++,name,d,at);
    Unit& u=units.back();
    u.ownerId=owner;u.isStructure=structure;u.isWorker=worker;
    u.navGrid=&navigation;
    if(structure){u.radius=2.5f*(d.visual_scale/2.8f);u.height=1.45f*d.visual_scale;}
    return u;
}
Unit* RTSSkirmish::find(int id) {
    for(auto& u:units)if(u.id==id)return &u;
    return nullptr;
}
const Unit* RTSSkirmish::find(int id)const {
    for(const auto& u:units)if(u.id==id)return &u;
    return nullptr;
}
void RTSSkirmish::reset() {
    units.clear();minerals.clear();routes_.clear();jobs_.clear();releasedSupply_.clear();
    production_.clear();construction_.clear();navigation=NavigationGrid();
    nextId_=1;playerMinerals=220;supplyUsed=0;supplyCap=18;
    enemySpawnTimer_=14.0f;result=MatchResult::Playing;
    friendlyBaseId_=spawn(baseData(true),{-22,0,0},0,"Command Center",true).id;
    enemyBaseId_=spawn(baseData(false),{22,0,0},1,"Hybrid Hive",true).id;
    navigation.blockDisc({-22,0,0},2.7f);
    navigation.blockDisc({22,0,0},2.7f);
    for(int i=0;i<4;++i) {
        spawn(marine(),{-13,0,-4.5f+i*3.0f},0,"Marine "+std::to_string(i+1));
        ++supplyUsed;
    }
    for(int i=0;i<2;++i) {
        spawn(workerData(),{-18,0,-5.5f+i*11.0f},0,"Worker "+std::to_string(i+1),false,true);
        ++supplyUsed;
    }
    for(int i=0;i<2;++i)
        spawn(zealot(),{13,0,-3.5f+i*7.0f},1,"Zealot "+std::to_string(i+1));
    minerals.push_back({{-3,0,-12},750});
    minerals.push_back({{-3,0,12},750});
    minerals.push_back({{0,0,0},1000});
    for(const auto& p:minerals)navigation.blockDisc(p.position,1.8f);
}
bool RTSSkirmish::setRoute(Unit& u,const Vec3& to,bool attackMove) {
    if(!u.isAlive() || u.isStructure)return false;
    Vec3 clamped=navigation.clampWorld(to);
    auto path=navigation.findPath(u.position,clamped);
    if(path.empty())return false;
    Route order;order.points=std::move(path);order.attackMove=attackMove;
    routes_[u.id]=std::move(order);
    u.moveTo(routes_[u.id].points.front(),attackMove);
    return true;
}
bool RTSSkirmish::orderMove(const Vec3& destination,bool attackMove) {
    if(result!=MatchResult::Playing)return false;
    bool ok=false;
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        routes_.erase(u.id);jobs_.erase(u.id);
        if(setRoute(u,destination,attackMove))ok=true;
    }
    return ok;
}
bool RTSSkirmish::orderAttack(int targetId) {
    Unit* target=find(targetId);
    if(result!=MatchResult::Playing || !target || !target->isAlive() || target->ownerId==0)return false;
    bool any=false;
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        routes_.erase(u.id);jobs_.erase(u.id);
        u.attackTarget(targetId);any=true;
    }
    return any;
}
void RTSSkirmish::orderStop() {
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        routes_.erase(u.id);jobs_.erase(u.id);u.navAgent.stop();
        u.targetUnitId=-1;u.state=UnitState::IDLE;u.attackMoveEnabled=false;
    }
}
bool RTSSkirmish::orderGather(const Vec3& click) {
    if(result!=MatchResult::Playing)return false;
    size_t index=minerals.size();
    float best=3.5f;
    for(size_t i=0;i<minerals.size();++i){
        float d=distanceXZ(click,minerals[i].position);
        if(minerals[i].remaining>0 && d<best){index=i;best=d;}
    }
    if(index==minerals.size())return false;
    bool any=false;
    for(auto& u:units)if(u.isAlive() && u.isWorker && u.ownerId==0 && u.isSelected) {
        routes_.erase(u.id);jobs_.erase(u.id);
        if(setRoute(u,minerals[index].position,false)) {
            jobs_[u.id]={index,WorkerPhase::ToMineral,0.0f,0};
            any=true;
        }
    }
    return any;
}
void RTSSkirmish::advanceRoutes() {
    for(auto it=routes_.begin();it!=routes_.end();) {
        Unit* u=find(it->first);
        if(!u || !u->isAlive() || u->isStructure) {it=routes_.erase(it);continue;}
        Route& route=it->second;
        if(u->state==UnitState::CHASING || u->state==UnitState::ATTACKING){++it;continue;}
        if(u->state==UnitState::MOVING && !u->navAgent.isNavigationFinished(u->position)){++it;continue;}
        ++route.next;
        if(route.next>=route.points.size()){
            u->navAgent.stop();u->state=UnitState::IDLE;
            it=routes_.erase(it);
        }else{
            u->moveTo(route.points[route.next],route.attackMove);
            ++it;
        }
    }
}
void RTSSkirmish::updateGathering(float dt) {
    for(auto it=jobs_.begin();it!=jobs_.end();) {
        Unit* u=find(it->first);
        WorkerJob& job=it->second;
        if(!u || !u->isAlive() || job.mineralIndex>=minerals.size()) {
            it=jobs_.erase(it);continue;
        }
        auto& node=minerals[job.mineralIndex];
        const Unit* base=find(friendlyBaseId_);
        if(!base || !base->isAlive()){it=jobs_.erase(it);continue;}
        if(job.phase==WorkerPhase::ToMineral && routes_.count(u->id)==0) {
            if(node.remaining<=0){it=jobs_.erase(it);continue;}
            if(distanceXZ(u->position,node.position)<4.5f) {
                job.phase=WorkerPhase::Harvesting;job.timer=1.6f;
            } else if(!setRoute(*u,node.position,false)) {
                it=jobs_.erase(it);continue;
            }
        }else if(job.phase==WorkerPhase::Harvesting) {
            job.timer-=dt;
            if(job.timer<=0){
                job.cargo=std::min(10,node.remaining);node.remaining-=job.cargo;
                if(job.cargo>0 && setRoute(*u,base->position,false)) {
                    job.phase=WorkerPhase::ToBase;
                }else{it=jobs_.erase(it);continue;}
            }
        }else if(job.phase==WorkerPhase::ToBase && routes_.count(u->id)==0) {
            if(distanceXZ(u->position,base->position)<6.0f){
                playerMinerals+=job.cargo;job.cargo=0;
                if(node.remaining>0 && setRoute(*u,node.position,false))
                    job.phase=WorkerPhase::ToMineral;
                else {it=jobs_.erase(it);continue;}
            }else if(!setRoute(*u,base->position,false)){
                it=jobs_.erase(it);continue;
            }
        }
        ++it;
    }
}
bool RTSSkirmish::buildBarracks(const Vec3& target) {
    if(result!=MatchResult::Playing || playerMinerals<150)return false;
    bool hasWorker=false;
    Vec3 pos=navigation.clampWorld(target);
    for(const auto& u:units)if(u.isAlive() && u.isWorker && u.isSelected && u.ownerId==0 &&
        distanceXZ(u.position,pos)<=18){hasWorker=true;break;}
    if(!hasWorker)return false;
    if(pos.x<-44 || pos.x>44 || pos.z<-44 || pos.z>44)return false;
    for(const auto& u:units)if(u.isStructure && u.isAlive() &&
        distanceXZ(pos,u.position)<u.radius+3.2f)return false;
    for(const auto& m:minerals)if(distanceXZ(pos,m.position)<5.0f)return false;
    auto cell=navigation.toCell(pos);
    for(int z=cell.z-2;z<=cell.z+2;++z)
        for(int x=cell.x-2;x<=cell.x+2;++x)
            if(!navigation.walkable(x,z))return false;
    playerMinerals-=150;
    Unit& b=spawn(barracksData(),navigation.centerOf(cell),0,
                  "Barracks "+std::to_string(nextId_),true);
    construction_[b.id]=9.0f;
    navigation.blockDisc(b.position,2.25f);
    return true;
}
bool RTSSkirmish::queueAt(int id,bool worker,int cost,float seconds,int supply) {
    Unit* b=find(id);
    if(!b || !b->isAlive() || b->ownerId!=0 || construction_.count(id)>0 ||
       playerMinerals<cost || supplyUsed+supply>supplyCap)return false;
    int pending=0;
    for(const auto& p:production_)if(p.buildingId==id)++pending;
    if(pending>=5)return false;
    playerMinerals-=cost;supplyUsed+=supply;
    production_.push_back({id,worker,seconds,supply,cost});
    return true;
}
bool RTSSkirmish::queueMarine() {
    for(const auto& u:units)if(u.isStructure && u.isAlive() && u.ownerId==0 &&
        u.data.resourceName=="Barracks" && construction_.count(u.id)==0)
        if(queueAt(u.id,false,50,6.0f,1))return true;
    return false;
}
bool RTSSkirmish::queueWorker() {return queueAt(friendlyBaseId_,true,40,4.5f,1);}
void RTSSkirmish::updateProduction(float dt) {
    for(auto it=construction_.begin();it!=construction_.end();) {
        const Unit* u=find(it->first);
        if(!u || !u->isAlive()){it=construction_.erase(it);continue;}
        it->second-=dt;
        if(it->second<=0)it=construction_.erase(it);
        else ++it;
    }
    for(auto it=production_.begin();it!=production_.end();) {
        const Unit* building=find(it->buildingId);
        if(!building || !building->isAlive()){
            playerMinerals+=it->paidMinerals;
            supplyUsed-=it->reservedSupply;
            it=production_.erase(it);continue;
        }
        // Process a single training slot per building.
        bool front=true;
        for(auto p=production_.begin();p!=it;++p)
            if(p->buildingId==it->buildingId){front=false;break;}
        if(!front){++it;continue;}
        it->remaining-=dt;
        if(it->remaining>0){++it;continue;}
        Vec3 origin=building->position;
        Vec3 spawnPos=origin+Vec3(-5,0,3);
        auto route=navigation.findPath(spawnPos,spawnPos);
        if(route.empty()){++it;continue;}
        spawn(it->worker?workerData():marine(),navigation.clampWorld(spawnPos),0,
              std::string(it->worker?"Worker ":"Marine ")+std::to_string(nextId_),
              false,it->worker);
        it=production_.erase(it);
    }
}
int RTSSkirmish::countOwnerUnits(int owner) const {
    int n=0;for(const auto& u:units)if(u.isAlive() && !u.isStructure && u.ownerId==owner)++n;
    return n;
}
void RTSSkirmish::updateEnemy(float dt) {
    const Unit* hive=find(enemyBaseId_);
    const Unit* base=find(friendlyBaseId_);
    if(!hive || !hive->isAlive() || !base || !base->isAlive())return;
    enemySpawnTimer_-=dt;
    if(enemySpawnTimer_<=0 && countOwnerUnits(1)<12) {
        enemySpawnTimer_=16.0f;
        spawn(zealot(),{hive->position.x-5,0,hive->position.z+4},1,
              "Hybrid "+std::to_string(nextId_));
    }
    for(auto& u:units)if(u.ownerId==1 && u.isAlive() && !u.isStructure &&
                          u.state==UnitState::IDLE)u.attackTarget(friendlyBaseId_);
}
void RTSSkirmish::update(float dt) {
    if(result!=MatchResult::Playing)return;
    dt=std::clamp(dt,0.0f,0.1f);
    for(auto& u:units) {
        Vec3 previous=u.position;
        u.update(dt,units,48.0f);
        if(u.isAlive() && !u.isStructure) {
            auto cell=navigation.toCell(u.position);
            if(!navigation.walkable(cell.x,cell.z)) {
                u.position=previous;
                u.navAgent.stop(); // prevent stepping through an occupied cell
            }
        }
    }
    // Dead army units return their supply exactly once. Production still reserves
    // supply at queue time, even before the unit exists.
    for(const auto& u:units)if(u.ownerId==0 && !u.isStructure && !u.isAlive()) {
        if(releasedSupply_.insert(u.id).second)
            supplyUsed=std::max(0,supplyUsed-u.data.supply_cost);
    }
    advanceRoutes();
    updateGathering(dt);
    updateProduction(dt);
    updateEnemy(dt);
    const Unit* human=find(friendlyBaseId_),*enemy=find(enemyBaseId_);
    if(!human || !human->isAlive())result=MatchResult::Defeat;
    else if(!enemy || !enemy->isAlive())result=MatchResult::Victory;
}
int RTSSkirmish::activeWorkerCount()const {
    int n=0;for(const auto& u:units)if(u.isAlive() && u.isWorker && u.ownerId==0)++n;
    return n;
}
int RTSSkirmish::completedBarracksCount()const {
    int n=0;
    for(const auto& u:units)if(u.isAlive() && u.ownerId==0 && u.isStructure &&
        u.data.resourceName=="Barracks" && !construction_.count(u.id))++n;
    return n;
}
std::string RTSSkirmish::statusLine()const {
    std::ostringstream s;
    s<<"Minerals "<<playerMinerals<<" | Supply "<<supplyUsed<<"/"<<supplyCap
     <<" | Workers "<<activeWorkerCount()<<" | Barracks "<<completedBarracksCount()
     <<" | Queue "<<production_.size();
    if(result==MatchResult::Victory)s<<" | VICTORY - R to restart";
    else if(result==MatchResult::Defeat)s<<" | DEFEAT - R to restart";
    return s.str();
}
