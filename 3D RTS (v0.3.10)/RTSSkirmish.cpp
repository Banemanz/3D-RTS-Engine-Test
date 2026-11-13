#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <cstdint>

namespace {
float distanceXZ(const Vec3& a,const Vec3& b) {
    return (a-b).lengthXZ();
}
double metaNumber(const RTSProject& p,const char* name,double defaultValue) {
    const auto it=p.metadata.find(name);
    if(it==p.metadata.end() || !it->second.is<double>())return defaultValue;
    return it->second.get<double>();
}
bool boolField(const picojson::object& o,const char* name) {
    auto it=o.find(name);
    return it!=o.end() && it->second.is<bool>() && it->second.get<bool>();
}
std::string metaString(const RTSProject& p,const char* name,const std::string& fallback){
    const auto i=p.metadata.find(name);
    if(i!=p.metadata.end() && i->second.is<std::string>())return i->second.get<std::string>();
    return fallback;
}
}
RTSSkirmish::RTSSkirmish():project_(RTSProject::defaults()){reset();}
void RTSSkirmish::loadProject(const RTSProject& project){
    project.validate();
    project_=project;
    reset();
}
void RTSSkirmish::rebuildNavigation(){
    const auto& t=project_.terrain;
    navigation=NavigationGrid(t.width(),t.depth(),t.spacing(),
                              t.minX()-t.spacing()*0.5f,t.minZ()-t.spacing()*0.5f);
    for(int z=0;z<t.depth();++z)for(int x=0;x<t.width();++x)
        navigation.setBlocked(x,z,t.cell(x,z).blocked);
    for(const auto& u:units)if(u.isStructure && u.isAlive())
        navigation.blockDisc(u.position,u.radius+0.2f);
    for(const auto& m:minerals)if(m.remaining>0)
        navigation.blockDisc(m.position,1.8f);
}
void RTSSkirmish::syncTerrain(const TerrainMap& t){
    project_.terrain=t;
    routes_.clear();queued_.clear();jobs_.clear();
    for(auto& u:units){u.navAgent.stop();if(!u.isStructure)u.state=UnitState::IDLE;
        u.position.y=project_.terrain.sampleHeight(u.position.x,u.position.z);}
    for(auto& m:minerals)m.position.y=project_.terrain.sampleHeight(m.position.x,m.position.z);
    rebuildNavigation();
}
Unit& RTSSkirmish::spawn(const UnitData& d,const Vec3& at,int owner,
                       const std::string& name,bool structure,bool worker,const std::string& catalogId) {
    units.emplace_back(nextId_++,name,d,at);
    Unit& u=units.back();
    u.ownerId=owner;u.isStructure=structure;u.isWorker=worker;
    u.catalogId=catalogId;
    if(!catalogId.empty()) {
        auto fields=project_.catalog.resolvedFields(catalogId);
        auto actor=fields.find("actor");
        if(actor!=fields.end() && actor->second.is<std::string>()) {
            ActorVisual visual=actorFromCatalog(project_.catalog,actor->second.get<std::string>());
            u.modelScale=visual.scale;
            u.actorOpacity=visual.opacity;
            u.actorMesh=visual.mesh;
        }
    }
    u.position.y=project_.terrain.sampleHeight(at.x,at.z);
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
    units.clear();minerals.clear();routes_.clear();queued_.clear();jobs_.clear();
    releasedSupply_.clear();production_.clear();construction_.clear();
    nextId_=1;friendlyBaseId_=-1;enemyBaseId_=-1;
    observedDead_.clear();elapsedSeconds_=0.0;triggerMessage.clear();
    marineId_=metaString(project_,"player_marine","marine");
    workerId_=metaString(project_,"player_worker","worker");
    enemyId_=metaString(project_,"enemy_soldier","zealot");
    barracksId_=metaString(project_,"player_barracks","barracks");
    playerBaseCatalog_=metaString(project_,"player_base","command_center");
    enemyBaseCatalog_=metaString(project_,"enemy_base","hive");
    playerMinerals=std::max(0,int(metaNumber(project_,"player_starting_minerals",220)));
    supplyCap=std::max(0,int(metaNumber(project_,"player_supply_cap",18)));
    enemyInterval_=float(std::clamp(metaNumber(project_,"enemy_spawn_interval",16.0),1.0,1000.0));
    supplyUsed=0;enemySpawnTimer_=enemyInterval_;result=MatchResult::Playing;
    rebuildNavigation();
    for(const auto& placement:project_.placements) {
        Vec3 loc=project_.terrain.clampToMap(placement.position);
        if(placement.kind=="Mineral"){
            minerals.push_back({loc,placement.amount});
            continue;
        }
        if(placement.kind!="Unit")continue; // future placed doodads do not affect gameplay yet
        const auto fields=project_.catalog.resolvedFields(placement.catalogId);
        bool structure=boolField(fields,"is_structure");
        bool worker=boolField(fields,"is_worker");
        UnitData data=unitFromCatalog(project_.catalog,placement.catalogId);
        Unit& instance=spawn(data,loc,placement.owner,placement.name.empty()?data.resourceName:placement.name,
                             structure,worker,placement.catalogId);
        if(placement.owner==0 && !structure)supplyUsed+=data.supply_cost;
        if(instance.catalogId==playerBaseCatalog_ && instance.ownerId==0)friendlyBaseId_=instance.id;
        if(instance.catalogId==enemyBaseCatalog_ && instance.ownerId==1)enemyBaseId_=instance.id;
        if(structure)navigation.blockDisc(instance.position,instance.radius+0.2f);
    }
    for(const auto& node:minerals)navigation.blockDisc(node.position,1.8f);
    triggers_.configure(project_);
    triggers_.start(*this);
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
bool RTSSkirmish::orderMove(const Vec3& destination,bool attackMove,bool append) {
    if(result!=MatchResult::Playing)return false;
    bool ok=false;
    Vec3 clamped=navigation.clampWorld(destination);
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        jobs_.erase(u.id);
        if(append && routes_.count(u.id)) {
            queued_[u.id].push_back({clamped,attackMove});
            ok=true;
        } else {
            routes_.erase(u.id);
            queued_.erase(u.id);
            if(setRoute(u,clamped,attackMove))ok=true;
        }
    }
    return ok;
}
bool RTSSkirmish::orderAttack(int targetId) {
    Unit* target=find(targetId);
    if(result!=MatchResult::Playing || !target || !target->isAlive() || target->ownerId==0)return false;
    bool any=false;
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        routes_.erase(u.id);queued_.erase(u.id);jobs_.erase(u.id);
        u.attackTarget(targetId);any=true;
    }
    return any;
}
void RTSSkirmish::orderStop() {
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        routes_.erase(u.id);queued_.erase(u.id);jobs_.erase(u.id);u.navAgent.stop();
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
        routes_.erase(u.id);queued_.erase(u.id);jobs_.erase(u.id);
        if(setRoute(u,minerals[index].position,false)) {
            jobs_[u.id]={index,WorkerPhase::ToMineral,0.0f,0};
            any=true;
        }
    }
    return any;
}
void RTSSkirmish::advanceRoutes() {
    std::vector<int> completed;
    for(auto it=routes_.begin();it!=routes_.end();) {
        Unit* u=find(it->first);
        if(!u || !u->isAlive() || u->isStructure) {it=routes_.erase(it);continue;}
        Route& route=it->second;
        if(u->state==UnitState::CHASING || u->state==UnitState::ATTACKING){++it;continue;}
        if(u->state==UnitState::MOVING && !u->navAgent.isNavigationFinished(u->position)){++it;continue;}
        ++route.next;
        if(route.next>=route.points.size()){
            u->navAgent.stop();u->state=UnitState::IDLE;
            completed.push_back(u->id);
            it=routes_.erase(it);
        }else{
            u->moveTo(route.points[route.next],route.attackMove);
            ++it;
        }
    }
    for(int id:completed) {
        auto q=queued_.find(id);
        if(q==queued_.end() || q->second.empty())continue;
        PendingMove next=q->second.front();
        q->second.pop_front();
        if(q->second.empty())queued_.erase(q);
        if(Unit* u=find(id))setRoute(*u,next.target,next.attackMove);
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
    const auto barracksData=unitFromCatalog(project_.catalog,barracksId_);
    const int price=barracksData.mineral_cost;
    if(result!=MatchResult::Playing || playerMinerals<price)return false;
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
    playerMinerals-=price;
    Unit& b=spawn(unitFromCatalog(project_.catalog,barracksId_),navigation.centerOf(cell),0,
                  "Barracks "+std::to_string(nextId_),true,false,barracksId_);
    construction_[b.id]=barracksData.build_time;
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
        u.catalogId==barracksId_ && construction_.count(u.id)==0) {
        auto d=unitFromCatalog(project_.catalog,marineId_);
        if(queueAt(u.id,false,d.mineral_cost,d.build_time,d.supply_cost))return true;
    }
    return false;
}
bool RTSSkirmish::queueWorker() {
    auto d=unitFromCatalog(project_.catalog,workerId_);
    return queueAt(friendlyBaseId_,true,d.mineral_cost,d.build_time,d.supply_cost);
}
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
        spawn(unitFromCatalog(project_.catalog,it->worker?workerId_:marineId_),navigation.clampWorld(spawnPos),0,
              std::string(it->worker?"Worker ":"Marine ")+std::to_string(nextId_),
              false,it->worker,it->worker?workerId_:marineId_);
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
        enemySpawnTimer_=enemyInterval_;
        spawn(unitFromCatalog(project_.catalog,enemyId_),{hive->position.x-5,0,hive->position.z+4},1,
              "Hybrid "+std::to_string(nextId_),false,false,enemyId_);
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
            u.position.y=project_.terrain.sampleHeight(u.position.x,u.position.z);
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
    elapsedSeconds_+=dt;
    std::vector<int> killed;
    for(const auto& unit:units)if(!unit.isAlive() && observedDead_.insert(unit.id).second)
        killed.push_back(unit.id);
    triggers_.tick(*this,elapsedSeconds_,killed);
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
        u.catalogId==barracksId_ && !construction_.count(u.id))++n;
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

void RTSSkirmish::addMineralsForTrigger(int amount) {
    const std::int64_t updated=static_cast<std::int64_t>(playerMinerals)+amount;
    playerMinerals=static_cast<int>(std::clamp(updated,std::int64_t(0),std::int64_t(100000000)));
}
bool RTSSkirmish::spawnForTrigger(const std::string& id,int owner,const Vec3& target) {
    if(owner<0 || owner>15 || units.size()>=4000 ||
       !project_.terrain.insideWorld(target.x,target.z))return false;
    auto entry=project_.catalog.find(id);
    if(!entry || entry->type!="Unit")return false;
    const auto fields=project_.catalog.resolvedFields(id);
    bool structure=boolField(fields,"is_structure");
    bool worker=boolField(fields,"is_worker");
    auto cell=navigation.toCell(target);
    if(!navigation.walkable(cell.x,cell.z))return false;
    UnitData stats=unitFromCatalog(project_.catalog,id);
    Unit& unit=spawn(stats,target,owner,
             id+" (trigger "+std::to_string(nextId_)+")",
             structure,worker,id);
    if(structure)navigation.blockDisc(unit.position,unit.radius+.2f);
    if(owner==0 && !structure)supplyUsed+=stats.supply_cost;
    return true;
}
void RTSSkirmish::endMatchForTrigger(bool victory) {
    result=victory?MatchResult::Victory:MatchResult::Defeat;
}
void RTSSkirmish::showMessageForTrigger(const std::string& message) {
    triggerMessage=message.substr(0,256);
}
int RTSSkirmish::countUnitsForTrigger(int owner,const std::string& type)const {
    int n=0;
    for(const auto& unit:units)if(unit.isAlive() &&
        (owner<0 || unit.ownerId==owner) &&
        (type.empty() || unit.catalogId==type))++n;
    return n;
}
int RTSSkirmish::ownerForTrigger(int id)const {
    const Unit* u=find(id);
    return u?u->ownerId:-999;
}
const ProjectPlacement* RTSSkirmish::regionForTrigger(const std::string& id)const {
    for(const auto& p:project_.placements)if(p.id==id && p.kind=="Region")return &p;
    return nullptr;
}
