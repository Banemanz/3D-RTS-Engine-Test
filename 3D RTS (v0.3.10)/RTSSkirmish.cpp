#include "RTSSkirmish.hpp"
#include "InstanceOverrides.hpp"
#include "RTSFormation.hpp"
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
    const double n=it->second.get<double>();
    return std::isfinite(n)?n:defaultValue;
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
    // Authoring parameters use rise/run (height change / horizontal distance).
    // Existing manually painted pathing blocks always take precedence.
    const float hardSlope=float(std::clamp(metaNumber(
        project_,"max_walkable_slope",1.25),0.05,100.0));
    const float costScale=float(std::clamp(metaNumber(
        project_,"slope_cost_multiplier",2.0),0.0,10.0));
    for(int z=0;z<t.depth();++z)for(int x=0;x<t.width();++x) {
        const float steepest=t.steepestSlope(x,z);
        navigation.setBlocked(x,z,t.cell(x,z).blocked || steepest>hardSlope);
        navigation.setTraversalCost(x,z,1.f+steepest*costScale);
    }
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
    vision.update(project_.terrain,units);
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
    units.clear();minerals.clear();routes_.clear();queued_.clear();jobs_.clear();rallyPoints_.clear();
    releasedSupply_.clear();production_.clear();construction_.clear();
    nextId_=1;trainingPage_=0;occupancyDirty_=false;friendlyBaseId_=-1;enemyBaseId_=-1;
    observedDead_.clear();elapsedSeconds_=0.0;triggerMessage.clear();
    visionIntervalTimer_=0.f;
    vision.reset(project_.terrain);
    // Modern starter maps use original unit IDs. Older authored projects
    // retain their embedded catalogs and old skirmish role identifiers.
    rangerId_=metaString(project_,"player_ranger",
        metaString(project_,"player_marine",
            project_.catalog.find("ranger")?"ranger":"marine"));
    workerId_=metaString(project_,"player_worker","worker");
    enemyId_=metaString(project_,"enemy_soldier",
        project_.catalog.find("rift_raider")?"rift_raider":"zealot");
    barracksId_=metaString(project_,"player_barracks","barracks");
    playerBaseCatalog_=metaString(project_,"player_base","command_center");
    enemyBaseCatalog_=metaString(project_,"enemy_base",
        project_.catalog.find("rift_bastion")?"rift_bastion":"hive");
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
        auto resolved=resolvePlacedUnit(project_,placement);
        const bool structure=resolved.isStructure;
        const bool worker=resolved.isWorker;
        const UnitData& data=resolved.unit;
        Unit& instance=spawn(data,loc,placement.owner,placement.name.empty()?data.resourceName:placement.name,
                             structure,worker,placement.catalogId);
        if(resolved.hasActor) {
            instance.modelScale=resolved.actor.scale;
            instance.actorOpacity=resolved.actor.opacity;
            instance.actorMesh=resolved.actor.mesh;
        }
        instance.modelScale=Vec3(instance.modelScale.x*placement.scale.x,
                                 instance.modelScale.y*placement.scale.y,
                                 instance.modelScale.z*placement.scale.z);
        instance.facingYawRad=toRadians(placement.rotation.y);
        instance.radius*=std::max(placement.scale.x,placement.scale.z);
        instance.height*=placement.scale.y;
        if(placement.owner==0 && !structure)supplyUsed+=data.supply_cost;
        if(instance.catalogId==playerBaseCatalog_ && instance.ownerId==0)friendlyBaseId_=instance.id;
        if(instance.catalogId==enemyBaseCatalog_ && instance.ownerId==1)enemyBaseId_=instance.id;
        if(structure)navigation.blockDisc(instance.position,instance.radius+0.2f);
    }
    for(const auto& node:minerals)navigation.blockDisc(node.position,1.8f);
    triggers_.configure(project_);
    triggers_.start(*this);
    vision.update(project_.terrain,units);
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
    std::vector<RTSFormationMember> selected;
    for(const auto& u:units)if(u.isAlive() && u.isSelected &&
                                u.ownerId==0 && !u.isStructure)
        selected.push_back({u.id,u.position,u.radius});
    const auto formation=arrangeRTSFormation(selected,navigation.clampWorld(destination),
        float(std::clamp(metaNumber(project_,"formation_spacing_multiplier",2.4),
                         1.5,6.0)));
    std::size_t assigned=0;
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        // Unit-order stable: assignments correspond to selected units in vector
        // order, with deterministic front-to-back/lateral slot arrangement.
        const Vec3 slot=navigation.clampWorld(formation[assigned++].destination);
        jobs_.erase(u.id);
        if(append && routes_.count(u.id)) {
            routes_[u.id].patrol=false; // queued move exits patrol after this leg
            queued_[u.id].push_back({slot,attackMove});
            ok=true;
        } else {
            routes_.erase(u.id);
            queued_.erase(u.id);
            if(setRoute(u,slot,attackMove))ok=true;
        }
    }
    return ok;
}
bool RTSSkirmish::orderPatrol(const Vec3& destination) {
    if(result!=MatchResult::Playing)return false;
    bool issued=false;
    std::vector<RTSFormationMember> selected;
    for(const auto& u:units)if(u.isAlive() && u.isSelected &&
                                u.ownerId==0 && !u.isStructure)
        selected.push_back({u.id,u.position,u.radius});
    const auto formation=arrangeRTSFormation(selected,navigation.clampWorld(destination),
        float(std::clamp(metaNumber(project_,"formation_spacing_multiplier",2.4),
                         1.5,6.0)));
    std::size_t assigned=0;
    for(auto& unit:units)if(unit.isAlive() && unit.isSelected && unit.ownerId==0 &&
                           !unit.isStructure){
        const Vec3 start=unit.position;
        const Vec3 goal=navigation.clampWorld(formation[assigned++].destination);
        routes_.erase(unit.id);
        queued_.erase(unit.id);
        jobs_.erase(unit.id);
        if(setRoute(unit,goal,true)){
            auto& route=routes_[unit.id];
            route.patrol=true;
            route.patrolStart=start;
            route.patrolGoal=goal;
            route.returning=false;
            issued=true;
        }
    }
    return issued;
}
bool RTSSkirmish::setRallyPoint(const Vec3& destination) {
    if(result!=MatchResult::Playing)return false;
    for(const auto& u:units)if(u.isAlive() && u.ownerId==0 &&
                               u.isSelected && !u.isStructure)return false;
    const Vec3 target=navigation.clampWorld(destination);
    const auto cell=navigation.toCell(target);
    if(!navigation.walkable(cell.x,cell.z))return false;
    bool set=false;
    for(const auto& u:units)if(u.isAlive() && u.ownerId==0 &&
                               u.isSelected && u.isStructure &&
                               !trainableUnits(u.id).empty()) {
        rallyPoints_[u.id]=target;set=true;
    }
    return set;
}
int RTSSkirmish::pendingProduction(int buildingId)const {
    int count=0;
    for(const auto& p:production_)if(p.buildingId==buildingId)++count;
    return count;
}
float RTSSkirmish::productionProgress(int buildingId) const {
    for(const auto& p:production_)if(p.buildingId==buildingId)
        return p.totalTime>0.f?std::clamp(1.f-p.remaining/p.totalTime,0.f,1.f):1.f;
    return -1.f;
}
bool RTSSkirmish::cancelSelectedProduction() {
    if(result!=MatchResult::Playing)return false;
    for(const auto& u:units)if(u.isSelected && u.isAlive() &&
                               u.ownerId==0 && u.isStructure) {
        for(auto it=production_.end();it!=production_.begin();) {
            --it;
            if(it->buildingId!=u.id)continue;
            playerMinerals+=it->paidMinerals;
            supplyUsed=std::max(0,supplyUsed-it->reservedSupply);
            production_.erase(it);
            return true;
        }
    }
    return false;
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
void RTSSkirmish::orderHold() {
    if(result!=MatchResult::Playing)return;
    for(auto& u:units)if(u.isAlive() && u.ownerId==0 &&
                          u.isSelected && !u.isStructure) {
        routes_.erase(u.id);
        queued_.erase(u.id);
        jobs_.erase(u.id);
        u.holdPosition();
    }
}

void RTSSkirmish::orderStop() {
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        routes_.erase(u.id);queued_.erase(u.id);jobs_.erase(u.id);u.navAgent.stop();
        u.targetUnitId=-1;u.state=UnitState::IDLE;u.attackMoveEnabled=false;
        u.holdPositionEnabled=false;
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
        if(u->state==UnitState::CHASING || u->state==UnitState::ATTACKING){
            route.engagementInterrupted=true;
            ++it;continue;
        }
        if(route.attackMove && route.engagementInterrupted) {
            // Combat pulled the unit off its A-move/patrol path. Do not
            // advance to the next waypoint as though the old one was reached.
            // Replan to the current leg's final destination from here.
            route.engagementInterrupted=false;
            auto resumed=navigation.findPath(u->position,route.points.back());
            if(!resumed.empty()) {
                route.points=std::move(resumed);
                route.next=0;
                u->moveTo(route.points.front(),true);
                ++it;continue;
            }
            // In case a newly placed obstruction makes the old destination
            // unreachable, retire this leg and attempt the next queued order.
            route.next=route.points.size();
        }
        if(u->state==UnitState::MOVING && !u->navAgent.isNavigationFinished(u->position)){++it;continue;}
        ++route.next;
        if(route.next>=route.points.size()){
            if(route.patrol){
                const Vec3 target=route.returning?route.patrolGoal:route.patrolStart;
                auto path=navigation.findPath(u->position,target);
                if(!path.empty()){
                    route.points=std::move(path);
                    route.next=0;
                    route.returning=!route.returning;
                    u->moveTo(route.points.front(),true);
                    ++it;
                    continue;
                }
            }
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
                if(node.remaining==0 && job.cargo>0)occupancyDirty_=true;
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
    const auto& terrain=project_.terrain;
    const float margin=std::max(2.f,terrain.spacing()*2.f);
    if(pos.x<terrain.minX()+margin ||
       pos.x>terrain.minX()+(terrain.width()-1)*terrain.spacing()-margin ||
       pos.z<terrain.minZ()+margin ||
       pos.z>terrain.minZ()+(terrain.depth()-1)*terrain.spacing()-margin)
        return false;
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
std::vector<std::string> RTSSkirmish::trainableUnits(int buildingId) const {
    const Unit* producer=find(buildingId);
    if(!producer || !producer->isAlive() || !producer->isStructure ||
       producer->ownerId!=0)return {};
    const auto fields=project_.catalog.resolvedFields(producer->catalogId);
    auto f=fields.find("trainable_units");
    if(f!=fields.end()) {
        std::vector<std::string> ids;
        if(!f->second.is<picojson::array>())return ids; // project validator reports this
        for(const auto& candidate:f->second.get<picojson::array>())
            if(candidate.is<std::string>())ids.push_back(candidate.get<std::string>());
        return ids;
    }
    // Backward compatibility for projects saved before trainable_units existed.
    if(producer->catalogId==playerBaseCatalog_)return {workerId_};
    if(producer->catalogId==barracksId_)return {rangerId_};
    return {};
}
int RTSSkirmish::selectedProducerId() const {
    int selected=-1;
    for(const auto& unit:units)if(unit.isSelected && unit.isAlive() && unit.ownerId==0) {
        if(!unit.isStructure || selected!=-1)return -1;
        selected=unit.id;
    }
    return selected;
}
int RTSSkirmish::trainingPageCount() const {
    const int producer=selectedProducerId();
    if(producer<0)return 0;
    const auto count=trainableUnits(producer).size();
    return count==0?0:int((count+2)/3);
}
int RTSSkirmish::trainingPage() const {
    const int pages=trainingPageCount();
    return pages==0?0:trainingPage_%pages;
}
bool RTSSkirmish::nextTrainingPage() {
    const int pages=trainingPageCount();
    if(pages<=1)return false;
    trainingPage_=(trainingPage()+1)%pages;
    return true;
}
std::string RTSSkirmish::selectedTrainableId(int slot) const {
    const int producer=selectedProducerId();
    if(producer<0 || slot<0 || slot>2)return "";
    auto choices=trainableUnits(producer);
    const std::size_t index=std::size_t(trainingPage()*3+slot);
    return index<choices.size()?choices[index]:"";
}
bool RTSSkirmish::canQueueAt(int id,const std::string& unitId) const {
    const Unit* building=find(id);
    if(result!=MatchResult::Playing || !building || !building->isAlive() ||
       !building->isStructure || building->ownerId!=0 ||
       construction_.count(id)>0)return false;
    auto choices=trainableUnits(id);
    if(std::find(choices.begin(),choices.end(),unitId)==choices.end())return false;
    const UnitData data=unitFromCatalog(project_.catalog,unitId);
    if(playerMinerals<data.mineral_cost ||
       supplyUsed+data.supply_cost>supplyCap)return false;
    int pending=0;
    for(const auto& p:production_)if(p.buildingId==id)++pending;
    return pending<5;
}
bool RTSSkirmish::canQueueSelectedTrainingSlot(int slot) const {
    const int producer=selectedProducerId();
    if(producer<0)return false;
    const auto id=selectedTrainableId(slot);
    return !id.empty() && canQueueAt(producer,id);
}
bool RTSSkirmish::queueAt(int id,const std::string& unitId) {
    if(!canQueueAt(id,unitId))return false;
    const UnitData data=unitFromCatalog(project_.catalog,unitId);
    playerMinerals-=data.mineral_cost;
    supplyUsed+=data.supply_cost;
    production_.push_back({id,unitId,data.build_time,
                           data.supply_cost,data.mineral_cost,data.build_time});
    return true;
}
bool RTSSkirmish::queueSelectedTrainingSlot(int slot) {
    if(!canQueueSelectedTrainingSlot(slot))return false;
    return queueAt(selectedProducerId(),selectedTrainableId(slot));
}
bool RTSSkirmish::queueRanger() {
    for(const auto& u:units)if(u.isStructure && u.isAlive() && u.ownerId==0 &&
        u.catalogId==barracksId_ && queueAt(u.id,rangerId_))return true;
    return false;
}
bool RTSSkirmish::queueWorker() {
    return queueAt(friendlyBaseId_,workerId_);
}
bool RTSSkirmish::findProductionSpawn(const Unit& producer,const UnitData& type,
                                      Vec3& out) const {
    // Deterministic bounded outlet search: first try the producer's rally
    // direction, then adjacent arcs and a second ring. No per-map exhaustive
    // scan and no spawns inside static navigation blockers or existing units.
    const float unitRadius=std::max(0.5f,0.62f*type.visual_scale);
    Vec3 forward{-1.f,0.f,0.35f};
    auto rally=rallyPoints_.find(producer.id);
    if(rally!=rallyPoints_.end()) {
        forward=rally->second-producer.position;forward.y=0;
        if(forward.lengthXZ()<0.01f)forward={-1.f,0.f,0.35f};
    }
    const float angle=std::atan2(forward.z,forward.x);
    constexpr float tau=6.2831853071795864769f;
    const float base=producer.radius+unitRadius+0.75f;
    for(int ring=0;ring<2;++ring)for(int k=0;k<12;++k) {
        // Alternate left/right angles so the chosen spawn stays near rally.
        const int side=(k%2==0)?1:-1;
        const int step=(k+1)/2;
        const float direction=angle+side*step*(tau/12.f);
        const float radius=base+ring*2.5f;
        const Vec3 desired=producer.position+
            Vec3(std::cos(direction)*radius,0.f,std::sin(direction)*radius);
        const Vec3 inside=navigation.clampWorld(desired);
        if((inside-desired).lengthXZ()>0.1f)continue;
        const auto cell=navigation.toCell(inside);
        if(!navigation.walkable(cell.x,cell.z))continue;
        Vec3 spot=navigation.centerOf(cell);
        bool occupied=false;
        for(const auto& mobile:units)if(mobile.isAlive() && !mobile.isStructure &&
          (mobile.position-spot).lengthXZ()<mobile.radius+unitRadius+0.25f) {
            occupied=true;break;
        }
        if(occupied)continue;
        spot.y=project_.terrain.sampleHeight(spot.x,spot.z);
        out=spot;
        return true;
    }
    return false;
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
        const UnitData type=unitFromCatalog(project_.catalog,it->catalogId);
        Vec3 spawnPos{};
        // Finished jobs wait when every legal outlet is blocked.
        if(!findProductionSpawn(*building,type,spawnPos)){++it;continue;}
        const auto fields=project_.catalog.resolvedFields(it->catalogId);
        const bool worker=boolField(fields,"is_worker");
        const std::string prefix=worker?"Worker ":
            it->catalogId==rangerId_?"Ranger ":type.resourceName+" ";
        Unit& produced=spawn(type,spawnPos,0,
                             prefix+std::to_string(nextId_),false,worker,it->catalogId);
        const auto rally=rallyPoints_.find(it->buildingId);
        if(rally!=rallyPoints_.end())setRoute(produced,rally->second,false);
        it=production_.erase(it);
    }
}
int RTSSkirmish::countOwnerUnits(int owner) const {
    int n=0;for(const auto& u:units)if(u.isAlive() && !u.isStructure && u.ownerId==owner)++n;
    return n;
}
void RTSSkirmish::updateEnemy(float dt) {
    const Unit* rift_bastion=find(enemyBaseId_);
    const Unit* base=find(friendlyBaseId_);
    if(!rift_bastion || !rift_bastion->isAlive() || !base || !base->isAlive())return;
    enemySpawnTimer_-=dt;
    if(enemySpawnTimer_<=0 && countOwnerUnits(1)<12) {
        enemySpawnTimer_=enemyInterval_;
        const UnitData enemy=unitFromCatalog(project_.catalog,enemyId_);
        Vec3 spawnPos{};
        if(findProductionSpawn(*rift_bastion,enemy,spawnPos))
            spawn(enemy,spawnPos,1,"Hybrid "+std::to_string(nextId_),false,false,enemyId_);
    }
    for(auto& u:units)if(u.ownerId==1 && u.isAlive() && !u.isStructure &&
                          u.state==UnitState::IDLE)u.attackTarget(friendlyBaseId_);
}
void RTSSkirmish::update(float dt) {
    if(result!=MatchResult::Playing)return;
    dt=std::clamp(dt,0.0f,0.1f);
    spatial_.rebuild(units);
    // Two-phase combat: every actor gets its chance to fire before damage
    // is applied, making mutually lethal hits independent of update order.
    std::vector<CombatHit> pendingHits;
    pendingHits.reserve(units.size());
    for(std::size_t i=0;i<units.size();++i) {
        auto& u=units[i];
        Vec3 previous=u.position;
        u.update(dt,units,48.0f,&spatial_,&pendingHits);
        if(u.isAlive() && !u.isStructure) {
            auto cell=navigation.toCell(u.position);
            if(!navigation.walkable(cell.x,cell.z)) {
                u.position=previous;
                u.navAgent.stop(); // prevent stepping through an occupied cell
            }
            u.position.y=project_.terrain.sampleHeight(u.position.x,u.position.z);
        }
        if(u.isAlive())spatial_.moved(i,previous,u.position);
    }
    // Stable ordering prevents actor-vector reordering changing overkill.
    std::sort(pendingHits.begin(),pendingHits.end(),
        [](const CombatHit& a,const CombatHit& b) {
            return a.targetId!=b.targetId ? a.targetId<b.targetId : a.attackerId<b.attackerId;
        });
    for(const auto& hit:pendingHits)
        if(Unit* target=find(hit.targetId))target->takeDamage(hit.rawDamage,hit.attackerName);
    // Dead structures are no longer static navigation blockers. Rebuild just
    // once on each newly destroyed structure, not in every simulation tick.
    bool staticChanged=false;
    for(const auto& u:units)if(u.isStructure && !u.isAlive() &&
       observedDead_.count(u.id)==0){staticChanged=true;break;}
    if(staticChanged)rebuildNavigation();
    // Dead army units return their supply exactly once. Production still reserves
    // supply at queue time, even before the unit exists.
    for(const auto& u:units)if(u.ownerId==0 && !u.isStructure && !u.isAlive()) {
        if(releasedSupply_.insert(u.id).second)
            supplyUsed=std::max(0,supplyUsed-u.data.supply_cost);
    }
    advanceRoutes();
    updateGathering(dt);
    if(occupancyDirty_) {rebuildNavigation();occupancyDirty_=false;}
    updateProduction(dt);
    for(auto it=rallyPoints_.begin();it!=rallyPoints_.end();) {
        const Unit* building=find(it->first);
        if(!building || !building->isAlive())it=rallyPoints_.erase(it);
        else ++it;
    }
    updateEnemy(dt);
    elapsedSeconds_+=dt;
    std::vector<int> killed;
    for(const auto& unit:units)if(!unit.isAlive() && observedDead_.insert(unit.id).second)
        killed.push_back(unit.id);
    triggers_.tick(*this,elapsedSeconds_,killed);
    visionIntervalTimer_+=dt;
    if(visionIntervalTimer_>=.2f){
        visionIntervalTimer_=std::fmod(visionIntervalTimer_,.2f);
        vision.update(project_.terrain,units);
    }
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

bool RTSSkirmish::applyEffectForTrigger(const std::string& effectId,int subjectId){
    Unit* target=find(subjectId);
    if(!target || !target->isAlive())return false;
    const auto operations=compileEffect(project_.catalog,effectId);
    for(const auto& op:operations){
        if(!target->isAlive())break;
        if(op.kind==EffectOperationKind::Damage)
            target->takeDamage(op.amount,"Effect "+effectId);
        else
            target->currentHp=std::clamp(target->currentHp+op.amount,0.f,
                                         target->data.max_hp);
    }
    return true;
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
