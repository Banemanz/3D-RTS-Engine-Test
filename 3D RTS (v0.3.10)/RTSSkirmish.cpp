#include "RTSSkirmish.hpp"
#include "RTSRequirements.hpp"
#include <iomanip>
#include <cstdint>
#include "InstanceOverrides.hpp"
#include "RTSFormation.hpp"
#include "RTSProjectileCollision.hpp"
#include "RTSAIPlanner.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

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
    ++navigationGeneration_;
}
void RTSSkirmish::syncTerrain(const TerrainMap& t){
    // Editor terrain changes are not yet represented as a gameplay command.
    // Do not offer a replay falsely claiming it can reproduce that change.
    replayValid_=false;
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
            u.actorCues=visual.cues;
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
    commandStream_.clear();
    commandOutcomes_.clear();
    recordedCommands_.clear();
    checkpoints_.clear();
    tick_=0;
    navigationGeneration_=0;
    replayValid_=true;
    applyingCommand_=false;
    aiInputMode_=AIInputMode::Live;
    units.clear();minerals.clear();routes_.clear();queued_.clear();jobs_.clear();rallyPoints_.clear();
    releasedSupply_.clear();production_.clear();construction_.clear();
    periodicEffects_.clear();
    behaviors_.clear();
    pendingAreas_.clear();
    impactFlashes_.clear();
    actorCues_.clear();
    abilityReadyAt_.clear();
    nextId_=1;trainingPage_=0;abilityPage_=0;occupancyDirty_=false;friendlyBaseId_=-1;enemyBaseId_=-1;
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
            instance.actorCues=resolved.actor.cues;
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
    checkpoints_.push_back({0,stateDigest()});
}
bool RTSSkirmish::setRoute(Unit& u,const Vec3& to,bool attackMove) {
    if(!u.isAlive() || u.isStructure)return false;
    Vec3 clamped=navigation.clampWorld(to);
    auto path=navigation.findPath(u.position,clamped);
    if(path.empty())return false;
    Route order;order.points=std::move(path);order.attackMove=attackMove;
    order.requestedGoal=clamped;
    order.navigationGeneration=navigationGeneration_;
    order.gridRevision=navigation.revision();
    routes_[u.id]=std::move(order);
    u.moveTo(routes_[u.id].points.front(),attackMove);
    return true;
}
bool RTSSkirmish::orderMove(const Vec3& destination,bool attackMove,bool append) {
    if(!applyingCommand_)replayValid_=false;
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
    if(!applyingCommand_)replayValid_=false;
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
    if(!applyingCommand_)replayValid_=false;
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
    if(!applyingCommand_)replayValid_=false;
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
    if(!applyingCommand_)replayValid_=false;
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
    if(!applyingCommand_)replayValid_=false;
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
    if(!applyingCommand_)replayValid_=false;
    for(auto& u:units)if(u.isAlive() && u.isSelected && u.ownerId==0 && !u.isStructure) {
        routes_.erase(u.id);queued_.erase(u.id);jobs_.erase(u.id);u.navAgent.stop();
        u.targetUnitId=-1;u.state=UnitState::IDLE;u.attackMoveEnabled=false;
        u.holdPositionEnabled=false;
    }
}
bool RTSSkirmish::orderGather(const Vec3& click) {
    if(!applyingCommand_)replayValid_=false;
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
        // A structure, depleted mineral, or terrain edit can invalidate a
        // route while the unit is in transit. Only inspect/replan when the
        // grid actually changes, never on every simulation tick.
        const bool changed=(route.navigationGeneration!=navigationGeneration_) ||
                           (route.gridRevision!=navigation.revision());
        if(changed){
            route.navigationGeneration=navigationGeneration_;
            route.gridRevision=navigation.revision();
            if(!navigation.routeClear(u->position,route.points,route.next)){
                auto reroute=navigation.findPath(u->position,route.requestedGoal);
                if(!reroute.empty()){
                    route.points=std::move(reroute);
                    route.next=0;
                    route.engagementInterrupted=false;
                    u->moveTo(route.points.front(),route.attackMove);
                    ++it;
                    continue;
                }
                // A completely sealed route is retired instead of pinning
                // the unit forever against an unreachable waypoint.
                u->navAgent.stop();u->state=UnitState::IDLE;
                completed.push_back(u->id);
                it=routes_.erase(it);
                continue;
            }
        }
        if(route.attackMove && route.engagementInterrupted) {
            // Combat pulled the unit off its A-move/patrol path. Do not
            // advance to the next waypoint as though the old one was reached.
            // Replan to the current leg's final destination from here.
            route.engagementInterrupted=false;
            auto resumed=navigation.findPath(u->position,route.requestedGoal);
            if(!resumed.empty()) {
                route.points=std::move(resumed);
                route.next=0;
                route.navigationGeneration=navigationGeneration_;
                route.gridRevision=navigation.revision();
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
                    route.requestedGoal=target;
                    route.navigationGeneration=navigationGeneration_;
                    route.gridRevision=navigation.revision();
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
    if(!applyingCommand_)replayValid_=false;
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
    // Production RTS footprints must not be stamped onto active mobile
    // units. Otherwise the new dynamic nav blocker traps those units inside
    // their current cell and their current orders can never finish.
    for(const auto& u:units)if(u.isAlive() && !u.isStructure &&
        distanceXZ(pos,u.position)<2.5f+u.radius)return false;
    auto cell=navigation.toCell(pos);
    for(int z=cell.z-2;z<=cell.z+2;++z)
        for(int x=cell.x-2;x<=cell.x+2;++x)
            if(!navigation.walkable(x,z))return false;
    playerMinerals-=price;
    Unit& b=spawn(barracksData,navigation.centerOf(cell),0,
                  barracksData.resourceName+" "+std::to_string(nextId_),
                  true,false,barracksId_);
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
bool RTSSkirmish::meetsRequirement(const std::string& id,const Unit* caster,
                                   const Unit* target)const{
    return id.empty() || evaluateRequirement(
        compileRequirement(project_.catalog,id),caster,target);
}
bool RTSSkirmish::evaluateRequirement(const RTSRequirementDefinition& node,
                                      const Unit* caster,const Unit* target)const{
    switch(node.kind){
        case RTSRequirementKind::All:
            for(const auto& child:node.children)
                if(!evaluateRequirement(child,caster,target))return false;
            return true;
        case RTSRequirementKind::Any:
            for(const auto& child:node.children)
                if(evaluateRequirement(child,caster,target))return true;
            return false;
        case RTSRequirementKind::Not:
            return !evaluateRequirement(node.children.front(),caster,target);
        case RTSRequirementKind::Predicate:break;
    }
    const auto& rule=node.predicate;
    const Unit* subject=rule.subject==RTSValidatorSubject::Caster?caster:target;
    if(!subject || !subject->isAlive())return false;
    switch(rule.kind){
        case RTSValidatorKind::HealthBelowPercent:
            return double(subject->currentHp)*100. <
                   double(subject->data.max_hp)*double(rule.value);
        case RTSValidatorKind::EnergyAtLeast:
            return subject->currentEnergy>=rule.value;
        case RTSValidatorKind::HasBehavior:
            return behaviors_.find({subject->id,rule.behaviorId})!=behaviors_.end();
        case RTSValidatorKind::IsStructure:return subject->isStructure;
        case RTSValidatorKind::OwnerUnitCountAtLeast:
            return countUnitsForTrigger(subject->ownerId,rule.unitId)>=
                   static_cast<int>(rule.value);
    }
    return false;
}
bool RTSSkirmish::canQueueAt(int id,const std::string& unitId) const {
    const Unit* building=find(id);
    if(result!=MatchResult::Playing || !building || !building->isAlive() ||
       !building->isStructure || building->ownerId!=0 ||
       construction_.count(id)>0)return false;
    auto choices=trainableUnits(id);
    if(std::find(choices.begin(),choices.end(),unitId)==choices.end())return false;
    const auto trainFields=project_.catalog.resolvedFields(unitId);
    const auto requirement=trainFields.find("requirement");
    if(requirement!=trainFields.end()){
        if(!requirement->second.is<std::string>())return false;
        if(!meetsRequirement(requirement->second.get<std::string>(),building,nullptr))
            return false;
    }
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
    if(!applyingCommand_)replayValid_=false;
    if(!canQueueSelectedTrainingSlot(slot))return false;
    return queueAt(selectedProducerId(),selectedTrainableId(slot));
}
bool RTSSkirmish::queueRanger() {
    if(!applyingCommand_)replayValid_=false;
    for(const auto& u:units)if(u.isStructure && u.isAlive() && u.ownerId==0 &&
        u.catalogId==barracksId_ && queueAt(u.id,rangerId_))return true;
    return false;
}
bool RTSSkirmish::queueWorker() {
    if(!applyingCommand_)replayValid_=false;
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
        // Content authors control visible names through catalog data.
        // Runtime spawning must not hardcode a particular starter roster.
        const std::string prefix=type.resourceName+" ";
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
            spawn(enemy,spawnPos,1,enemy.resourceName+" "+
                  std::to_string(nextId_),false,false,enemyId_);
    }
    // Version-1 playback must retain its historical post-unit-update
    // direct AI order. New live planning happens *before* unit movement,
    // and v2 replays consume only commands from their recorded tape.
    if(aiInputMode_==AIInputMode::LegacyDirect)
        for(auto& u:units)if(u.ownerId==1 && u.isAlive() &&
            !u.isStructure && u.state==UnitState::IDLE)
            u.attackTarget(friendlyBaseId_);
}
void RTSSkirmish::planAIOrders(){
    if(aiInputMode_!=AIInputMode::Live)return;
    const auto aiEnabled=project_.metadata.find("enemy_ai_enabled");
    if(aiEnabled!=project_.metadata.end() && aiEnabled->second.is<bool>() &&
       !aiEnabled->second.get<bool>())return;
    const RTSAICombatPolicy policy{true,float(std::clamp(
        metaNumber(project_,"enemy_ai_guard_radius",14.0),0.0,80.0))};
    const auto decisions=planRTSEnemyAttackOrders(
        units,1,enemyBaseId_,friendlyBaseId_,tick_+1,policy);
    for(const auto& decision:decisions)
        // On queue saturation, a unit retries next tick; do not bypass
        // the replay log by modifying the unit directly.
        (void)queueCommand(decision);
}
void RTSSkirmish::emitActorCue(int unitId,RTSActorCueEvent event,
                                const Vec3& at){
    const Unit* subject=find(unitId);
    if(!subject)return;
    // Snapshot the definition at spawn/placement time. Watchers are visual
    // only: no script action, Effect, collision, or replay state may depend
    // on the emitted cue list or its eviction order.
    for(const auto& definition:subject->actorCues){
        if(definition.event!=event)continue;
        if(actorCues_.size()>=MaxActorCues)actorCues_.pop_front();
        actorCues_.push_back({definition,at,tick_,unitId});
    }
}
std::vector<RTSSkirmish::ActorCueWatch> RTSSkirmish::actorCueWatch(
    std::size_t limit)const{
    std::vector<ActorCueWatch> projected;
    projected.reserve(std::min(limit,actorCues_.size()));
    for(const auto& cue:actorCues_){
        if(projected.size()>=limit)break;
        const std::uint64_t elapsed=tick_>=cue.bornTick?
            tick_-cue.bornTick:cue.definition.durationTicks;
        if(elapsed>=cue.definition.durationTicks)continue;
        projected.push_back({cue.definition.event,cue.definition.style,
            cue.position,cue.definition.tint,cue.definition.radius,
            float(cue.definition.durationTicks-elapsed)/
                float(cue.definition.durationTicks),cue.unitId});
    }
    return projected;
}
void RTSSkirmish::update(float dt) {
    if(result!=MatchResult::Playing)return;
    dt=std::clamp(dt,0.0f,0.1f);
    if(std::fabs(dt-1.f/60.f)>0.000001f)replayValid_=false;
    ++tick_;
    while(!actorCues_.empty() &&
          tick_>=actorCues_.front().bornTick+
                 actorCues_.front().definition.durationTicks)
        actorCues_.pop_front();
    if(tick_>RTSReplayTape::MaxTicks)replayValid_=false;
    // Expired modifiers cannot affect this tick's casts or combat.
    updateBehaviors();
    const auto pending=commandStream_.take(tick_);
    for(const auto& command:pending){
        bool accepted=false;
        try{accepted=applyCommand(command);}
        catch(...){
            commandOutcomes_.push_back({tick_,command.sequence,command.kind,false,command.source});
            if(commandOutcomes_.size()>128)commandOutcomes_.pop_front();
            throw;
        }
        commandOutcomes_.push_back({tick_,command.sequence,command.kind,accepted,command.source});
        if(commandOutcomes_.size()>128)commandOutcomes_.pop_front();
        if(replayValid_){
            if(recordedCommands_.size()<32768)recordedCommands_.push_back(command);
            else replayValid_=false; // bounded tape, never silently truncated
        }
    }
    // Plan from the pre-movement snapshot: a nearby player unit may trigger
    // automatic tactical acquisition during Unit::update later this tick.
    planAIOrders();
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
    for(const auto& hit:pendingHits){
        if(const Unit* source=find(hit.attackerId))
            emitActorCue(hit.attackerId,RTSActorCueEvent::Attack,source->position);
        // All attacks have already been issued for this tick. Projectile
        // Weapons only defer the damage; old hitscan attacks remain instant.
        if(!hit.projectileId.empty() && queueWeaponProjectile(hit))continue;
        if(Unit* target=find(hit.targetId)){
            emitActorCue(hit.attackerId,RTSActorCueEvent::Impact,target->position);
            target->takeDamage(hit.rawDamage,hit.attackerName,hit.armorPiercing);
        }
    }
    // Delayed ground impact runs after movement/combat so moving out of the
    // radius matters, but before health/energy regeneration and periodic ticks.
    updateAreaImpacts();
    // Unit catalog passive regeneration is evaluated in the authoritative
    // fixed step, independently of rendering and active navigation.
    for(auto& unit:units)if(unit.isAlive()){
        if(unit.data.hp_regen>0.f)
            unit.currentHp=std::min(unit.data.max_hp,
                                    unit.currentHp+unit.data.hp_regen*dt);
        if(unit.data.energy_regen>0.f && unit.data.max_energy>0.f)
            unit.currentEnergy=std::min(unit.data.max_energy,
                                        unit.currentEnergy+unit.data.energy_regen*dt);
    }
    updatePeriodicEffects();
    // Eliminate behavior instances on dead targets without leaving a stale
    // speed/armor bonus attached to the Unit snapshot.
    updateBehaviors();
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
    for(const auto& unit:units)if(!unit.isAlive() && observedDead_.insert(unit.id).second){
        killed.push_back(unit.id);
        emitActorCue(unit.id,RTSActorCueEvent::Death,unit.position);
    }
    triggers_.tick(*this,elapsedSeconds_,killed);
    visionIntervalTimer_+=dt;
    if(visionIntervalTimer_>=.2f){
        visionIntervalTimer_=std::fmod(visionIntervalTimer_,.2f);
        vision.update(project_.terrain,units);
    }
    const Unit* human=find(friendlyBaseId_),*enemy=find(enemyBaseId_);
    if(!human || !human->isAlive())result=MatchResult::Defeat;
    else if(!enemy || !enemy->isAlive())result=MatchResult::Victory;
    if(replayValid_ && tick_%60==0)
        checkpoints_.push_back({tick_,stateDigest()});
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

std::vector<RTSSkirmish::BehaviorWatch> RTSSkirmish::behaviorWatch(
    std::size_t limit)const{
    std::vector<BehaviorWatch> list;
    const std::size_t cap=std::min(limit,std::size_t(256));
    list.reserve(std::min(cap,behaviors_.size()));
    for(const auto& item:behaviors_){
        if(list.size()>=cap)break;
        if(item.second.expiresTick<=tick_)continue;
        list.push_back({item.first.first,item.first.second,item.second.stacks,
                        item.second.expiresTick-tick_});
    }
    return list;
}
int RTSSkirmish::unitBehaviorCount(int id)const{
    int count=0;
    auto it=behaviors_.lower_bound({id,std::string{}});
    for(;it!=behaviors_.end() && it->first.first==id;++it)++count;
    return count;
}
int RTSSkirmish::unitBehaviorStacks(int id,const std::string& behavior)const{
    const auto it=behaviors_.find({id,behavior});
    return it==behaviors_.end()?0:it->second.stacks;
}
std::uint64_t RTSSkirmish::unitBehaviorTicksRemaining(
    int id,const std::string& behavior)const{
    const auto it=behaviors_.find({id,behavior});
    return it==behaviors_.end() || it->second.expiresTick<=tick_?
        0:it->second.expiresTick-tick_;
}
void RTSSkirmish::recalculateBehaviorStats(int targetId){
    Unit* target=find(targetId);
    if(!target)return;
    double armor=0.,damage=0.,speed=1.;
    auto it=behaviors_.lower_bound({targetId,std::string{}});
    for(;it!=behaviors_.end() && it->first.first==targetId;++it){
        const auto definition=compileBehavior(project_.catalog,it->first.second);
        armor+=double(it->second.stacks)*definition.armorBonus;
        damage+=double(it->second.stacks)*definition.damageBonus;
        speed+=double(it->second.stacks)*(definition.speedMultiplier-1.f);
    }
    target->behaviorArmorBonus=static_cast<float>(std::clamp(armor,-100.,100.));
    target->behaviorDamageBonus=static_cast<float>(std::clamp(damage,-500.,500.));
    target->behaviorSpeedFactor=static_cast<float>(std::clamp(speed,.1,3.));
    target->navAgent.maxSpeed=target->data.move_speed*target->behaviorSpeedFactor;
}
void RTSSkirmish::applyBehavior(int targetId,const BehaviorDefinition& b){
    const auto key=std::make_pair(targetId,b.id);
    auto it=behaviors_.find(key);
    if(it==behaviors_.end())
        behaviors_.emplace(key,ActiveBehavior{1,tick_+b.durationTicks});
    else{
        it->second.stacks=std::min(b.maxStacks,it->second.stacks+1);
        if(b.refreshDuration)it->second.expiresTick=tick_+b.durationTicks;
    }
    recalculateBehaviorStats(targetId);
    if(const Unit* affected=find(targetId))
        emitActorCue(targetId,RTSActorCueEvent::Status,affected->position);
}
void RTSSkirmish::removeBehavior(int targetId,const std::string& id){
    auto it=behaviors_.find({targetId,id});
    if(it==behaviors_.end())return;
    if(!compileBehavior(project_.catalog,id).dispellable)return;
    behaviors_.erase(it);
    recalculateBehaviorStats(targetId);
}
void RTSSkirmish::updateBehaviors(){
    // Status count is bounded, but a 4000-Unit map can have many statuses.
    // For dense status maps, resolve alive Unit IDs in one linear pass rather
    // than 256 separate linear scans. For sparse statuses avoid the index cost.
    const bool dense=behaviors_.size()>32;
    std::unordered_set<int> aliveIds;
    if(dense){
        aliveIds.reserve(units.size());
        for(const auto& unit:units)if(unit.isAlive())aliveIds.insert(unit.id);
    }
    // std::map provides stable key order; recalc only changed target IDs.
    std::vector<int> changed;
    for(auto it=behaviors_.begin();it!=behaviors_.end();){
        const int id=it->first.first;
        const Unit* sparseTarget=dense?nullptr:find(id);
        const bool alive=dense?aliveIds.count(id)>0:
                           sparseTarget && sparseTarget->isAlive();
        if(!alive || tick_>=it->second.expiresTick){
            changed.push_back(it->first.first);
            it=behaviors_.erase(it);
        }else ++it;
    }
    std::sort(changed.begin(),changed.end());
    changed.erase(std::unique(changed.begin(),changed.end()),changed.end());
    for(const int id:changed)recalculateBehaviorStats(id);
}

void RTSSkirmish::updatePeriodicEffects(){
    for(auto it=periodicEffects_.begin();it!=periodicEffects_.end();){
        Unit* target=find(it->targetId);
        if(!target || !target->isAlive() || tick_>it->finalTick){
            it=periodicEffects_.erase(it);
            continue;
        }
        if(tick_>=it->nextTick){
            if(it->kind==EffectOperationKind::PeriodicDamage)
                target->takeDamage(it->amount,"Effect "+it->sourceEffect);
            else
                target->currentHp=std::min(target->data.max_hp,
                                            target->currentHp+it->amount);
            it->nextTick+=it->intervalTicks;
        }
        if(!target->isAlive() || it->nextTick>it->finalTick)
            it=periodicEffects_.erase(it);
        else ++it;
    }
}

bool RTSSkirmish::applyEffectForTrigger(const std::string& effectId,int subjectId){
    Unit* target=find(subjectId);
    if(!target || !target->isAlive())return false;
    const auto operations=compileEffect(project_.catalog,effectId);
    std::size_t pending=0;
    for(const auto& operation:operations)
        if(operation.kind==EffectOperationKind::PeriodicDamage ||
           operation.kind==EffectOperationKind::PeriodicHeal)++pending;
    if(pending>MaxPeriodicEffects-periodicEffects_.size())
        return false; // atomic scheduling admission, no partial effect graph
    // Count new distinct status keys *before* applying anything. Multiple
    // ApplyBehavior steps in a Sequence to one key only consume one slot.
    std::set<std::string> incoming;
    for(const auto& operation:operations)
        if(operation.kind==EffectOperationKind::ApplyBehavior &&
           behaviors_.count({subjectId,operation.behaviorId})==0)
            incoming.insert(operation.behaviorId);
    if(incoming.size()>MaxActiveBehaviors-behaviors_.size())return false;
    for(const auto& op:operations){
        if(!target->isAlive())break;
        if(op.kind==EffectOperationKind::Damage)
            target->takeDamage(op.amount,"Effect "+effectId);
        else if(op.kind==EffectOperationKind::Heal)
            target->currentHp=std::min(target->data.max_hp,
                                       target->currentHp+op.amount);
        else if(op.kind==EffectOperationKind::ApplyBehavior)
            applyBehavior(subjectId,compileBehavior(project_.catalog,op.behaviorId));
        else if(op.kind==EffectOperationKind::RemoveBehavior)
            removeBehavior(subjectId,op.behaviorId);
        else
            periodicEffects_.push_back({subjectId,op.kind,op.amount,
                op.intervalTicks,tick_+op.intervalTicks,
                tick_+op.durationTicks,effectId});
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

int RTSSkirmish::abilityPageCount()const{
    const Unit* caster=nullptr;
    for(const auto& u:units)if(u.isSelected && u.isAlive() && u.ownerId==0){
        if(caster)return 0;
        caster=&u;
    }
    if(!caster || caster->catalogId.empty())return 0;
    const auto abilities=unitAbilityIds(project_.catalog,caster->catalogId);
    return static_cast<int>((abilities.size()+2)/3);
}
bool RTSSkirmish::nextAbilityPage(){
    const int count=abilityPageCount();
    if(count<=1)return false;
    abilityPage_=(abilityPage_+1)%count;
    return true;
}
std::string RTSSkirmish::selectedAbilityId(int slot) const {
    if(slot<0 || slot>=3)return "";
    const Unit* caster=nullptr;
    for(const auto& u:units)if(u.isSelected && u.isAlive() && u.ownerId==0){
        if(caster)return ""; // ambiguity: HUD abilities require one caster
        caster=&u;
    }
    if(!caster || caster->catalogId.empty())return "";
    const auto abilities=unitAbilityIds(project_.catalog,caster->catalogId);
    const int page=abilityPage_%std::max(1,static_cast<int>((abilities.size()+2)/3));
    const std::size_t index=static_cast<std::size_t>(page*3+slot);
    return index<abilities.size()?abilities[index]:"";
}
std::uint64_t RTSSkirmish::abilityCooldownRemaining(
    int casterId,const std::string& abilityId)const{
    auto it=abilityReadyAt_.find({casterId,abilityId});
    if(it==abilityReadyAt_.end() || it->second<=tick_)return 0;
    return it->second-tick_;
}
bool RTSSkirmish::canPrepareAbility(int casterId,
                                    const std::string& abilityId)const{
    if(result!=MatchResult::Playing)return false;
    const Unit* caster=find(casterId);
    if(!caster || !caster->isAlive() || caster->ownerId!=0 ||
       caster->catalogId.empty())return false;
    const auto options=unitAbilityIds(project_.catalog,caster->catalogId);
    if(std::find(options.begin(),options.end(),abilityId)==options.end())return false;
    const auto ability=compileAbility(project_.catalog,abilityId);
    return abilityCooldownRemaining(casterId,abilityId)==0 &&
           playerMinerals>=ability.mineralCost &&
           caster->currentEnergy>=float(ability.energyCost);
}
Vec3 RTSSkirmish::projectilePositionAt(const PendingAreaImpact& area,
                                        std::uint64_t atTick){
    if(!area.projectile || area.flightTicks==0)return area.center;
    const double t=std::clamp(
        double(atTick-area.launchTick)/double(area.flightTicks),0.,1.);
    const float f=static_cast<float>(t);
    Vec3 point=area.launchPosition+
        (area.destination-area.launchPosition)*f;
    point.y+=4.f*area.arcHeight*f*(1.f-f);
    return point;
}
std::vector<RTSSkirmish::ImpactFlashWatch> RTSSkirmish::impactFlashWatch(
    std::size_t limit)const{
    std::vector<ImpactFlashWatch> watch;
    for(const auto& flash:impactFlashes_){
        if(watch.size()>=limit)break;
        const std::uint64_t age=tick_>=flash.bornTick?
                                   tick_-flash.bornTick:FlashTicks;
        if(age>=FlashTicks)continue;
        watch.push_back({flash.center,flash.radius,
            float(FlashTicks-age)/float(FlashTicks),flash.applied});
    }
    return watch;
}
std::size_t RTSSkirmish::pendingWeaponShotCount()const{
    return static_cast<std::size_t>(std::count_if(pendingAreas_.begin(),
        pendingAreas_.end(),
        [](const PendingAreaImpact& a){return a.weaponShot;}));
}
std::vector<RTSSkirmish::AreaImpactWatch> RTSSkirmish::areaImpactWatch(
    std::size_t limit)const{
    std::vector<AreaImpactWatch> watch;
    watch.reserve(std::min({limit,pendingAreas_.size(),std::size_t(128)}));
    for(const auto& a:pendingAreas_){
        if(watch.size()>=limit)break;
        AreaImpactWatch item;
        item.center=a.center;item.radius=a.radius;
        item.shape=a.shape;item.shapeOrigin=a.shapeOrigin;
        item.areaAngle=a.areaAngle;item.areaWidth=a.areaWidth;
        item.remainingTicks=a.impactTick>tick_?a.impactTick-tick_:0;
        item.abilityId=a.abilityId;
        item.weaponShot=a.weaponShot;
        item.projectileVisible=a.projectile &&
            tick_<a.launchTick+a.flightTicks;
        if(item.projectileVisible){
            item.projectilePosition=projectilePositionAt(a,tick_);
            const auto earlier=tick_>3?tick_-3:0;
            item.projectileTrailStart=projectilePositionAt(
                a,std::max(a.launchTick,earlier));
            item.projectileTint=a.tint;
            item.projectileScale=a.visualScale;
        }
        watch.push_back(std::move(item));
    }
    return watch;
}
bool RTSSkirmish::canCastAbilityAt(int casterId,
                                    const std::string& abilityId,
                                    const Vec3& ground)const{
    if(!canPrepareAbility(casterId,abilityId))return false;
    const auto ability=compileAbility(project_.catalog,abilityId);
    if(ability.target!=AbilityTarget::Ground)return false;
    const Unit* caster=find(casterId);
    if(!caster || !std::isfinite(ground.x) || !std::isfinite(ground.z) ||
       !project_.terrain.insideWorld(ground.x,ground.z) ||
       !vision.visible(ground))return false;
    return (ground-caster->position).lengthXZ()<=ability.castRange &&
           meetsRequirement(ability.requirementId,caster,nullptr);
}
bool RTSSkirmish::canCastAbility(int casterId,const std::string& abilityId,
                                int targetId)const{
    if(!canPrepareAbility(casterId,abilityId))return false;
    const Unit* caster=find(casterId);
    const auto ability=compileAbility(project_.catalog,abilityId);
    if(ability.target==AbilityTarget::Self)
        return (targetId==-1 || targetId==casterId) &&
               meetsRequirement(ability.requirementId,caster,caster);
    if(ability.target==AbilityTarget::Ground)return false;
    const Unit* target=find(targetId);
    if(!target || !target->isAlive())return false;
    if(ability.target==AbilityTarget::Ally && target->ownerId!=caster->ownerId)
        return false;
    if(ability.target==AbilityTarget::Enemy && target->ownerId==caster->ownerId)
        return false;
    if(target->ownerId!=caster->ownerId && !vision.visible(target->position))
        return false;
    return (target->position-caster->position).lengthXZ()<=ability.castRange &&
           meetsRequirement(ability.requirementId,caster,target);
}
bool RTSSkirmish::applyAreaEffect(const PendingAreaImpact& area){
    // The target set is based on unit positions at the *impact* tick.
    // Stable ascending Unit IDs make Effect application and behavior
    // admission identical after Windows/Linux entity vector reorders.
    std::vector<int> targets;
    targets.reserve(64);
    for(const Unit& unit:units){
        if(!unit.isAlive())continue;
        if(area.team==AbilityAreaTeam::Enemy && unit.ownerId==area.owner)continue;
        if(area.team==AbilityAreaTeam::Ally && unit.ownerId!=area.owner)continue;
        const double dx=double(unit.position.x)-double(area.center.x);
        const double dz=double(unit.position.z)-double(area.center.z);
        if(rtsAreaContains(area.shape,dx,dz,
                double(area.center.x)-area.shapeOrigin.x,
                double(area.center.z)-area.shapeOrigin.z,
                area.radius,area.areaAngle,area.areaWidth))
            targets.push_back(unit.id);
    }
    std::sort(targets.begin(),targets.end());
    // Reject an oversized area *before* applying damage, status or healing.
    // Target cap protects mass-selection and effect-graph CPU/allocations.
    static constexpr std::size_t MaxAreaTargets=64;
    if(targets.size()>MaxAreaTargets)return false;
    if(targets.empty())return true; // cast into empty ground still costs energy
    const auto operations=compileEffect(project_.catalog,area.effectId);
    std::size_t periodicPerTarget=0;
    std::set<std::string> granted;
    for(const auto& op:operations){
        if(op.kind==EffectOperationKind::PeriodicDamage ||
           op.kind==EffectOperationKind::PeriodicHeal)++periodicPerTarget;
        if(op.kind==EffectOperationKind::ApplyBehavior)
            granted.insert(op.behaviorId);
    }
    const auto remainingPeriodic=MaxPeriodicEffects-periodicEffects_.size();
    if(periodicPerTarget>0 &&
       targets.size()>remainingPeriodic/periodicPerTarget)return false;
    std::size_t freshStatuses=0;
    for(int id:targets)for(const auto& status:granted)
        if(!behaviors_.count({id,status}))++freshStatuses;
    if(freshStatuses>MaxActiveBehaviors-behaviors_.size())return false;
    for(const int id:targets){
        // Bounded, preadmitted operations should never fail here. If earlier
        // targets kill others, applyEffectForTrigger safely ignores dead IDs.
        const Unit* unit=find(id);
        if(unit && unit->isAlive())
            if(!applyEffectForTrigger(area.effectId,id))return false;
    }
    return true;
}
bool RTSSkirmish::detectProjectileCollision(PendingAreaImpact& area){
    if(!area.projectile || (!area.collideUnits && !area.weaponShot) ||
       tick_<=area.launchTick ||
       tick_>area.launchTick+area.flightTicks)return false;
    const Vec3 previous=projectilePositionAt(area,tick_-1);
    const Vec3 current=projectilePositionAt(area,tick_);
    const Vec3 path=current-previous;
    // Only this phase may use spatial buckets: the fixed tick rebuilt
    // them before Unit updates and incrementally moved each live Unit.
    // Ground casts can run earlier in applyCommand, so their admission
    // deliberately keeps the full-list fallback.
    const float lengthXZ=path.lengthXZ();
    const Vec3 midpoint=previous+path*.5f;
    const float searchRadius=lengthXZ*.5f+
        spatial_.maxEntityRadius()+area.collisionRadius;
    const auto candidates=spatial_.nearby(midpoint,searchRadius);
    double best=2.;
    int bestId=0;
    // Exact earliest contact on each segment against the Unit's expanded
    // cylinder. Broadphase bounds are conservative even for scaled units.
    for(const auto index:candidates){
        const Unit& unit=units[index];
        if(!unit.isAlive())continue;
        if(area.weaponShot && !area.collideUnits &&
           unit.id!=area.attackTargetId)continue;
        if(area.team==AbilityAreaTeam::Enemy && unit.ownerId==area.owner)continue;
        if(area.team==AbilityAreaTeam::Ally && unit.ownerId!=area.owner)continue;
        double t=0.;
        if(!sweptProjectileCylinder(previous,current,unit.position,
                                   unit.radius,unit.height,
                                   area.collisionRadius,t))
            continue;
        // Segment parameter first, Unit ID second. Iteration order of the
        // underlying unit vector can never determine which object intercepts.
        if(t<best-1e-10 ||
           (std::fabs(t-best)<=1e-10 && (bestId==0 || unit.id<bestId))){
            best=t;bestId=unit.id;
        }
    }
    if(bestId<=0)return false;
    const Unit* hit=find(bestId);
    if(!hit)return false;
    // Contact burst centers on the intercepted Unit, ensuring the unit
    // actually hit receives the Area Effect even for a tiny area radius.
    area.center=hit->position;
    area.impactTick=tick_;
    if(area.weaponShot)area.contactTargetId=bestId;
    return true;
}
bool RTSSkirmish::queueWeaponProjectile(const CombatHit& hit){
    const Unit* attacker=find(hit.attackerId);
    const Unit* target=find(hit.targetId);
    if(!attacker || !target || !target->isAlive() ||
       pendingWeaponShotCount()>=MaxPendingWeaponShots)
        return false; // deterministic hitscan fallback at saturation
    const auto profile=compileProjectile(project_.catalog,hit.projectileId);
    PendingAreaImpact shot;
    shot.projectile=true;
    shot.weaponShot=true;
    shot.actorSourceId=hit.attackerId;
    shot.projectileId=hit.projectileId;
    shot.abilityId="Weapon: "+attacker->data.weaponId;
    shot.attackerId=hit.attackerId;
    shot.attackTargetId=hit.targetId;
    shot.attackerName=hit.attackerName;
    shot.attackDamage=hit.rawDamage;
    shot.armorPiercing=hit.armorPiercing;
    shot.owner=attacker->ownerId;
    shot.team=AbilityAreaTeam::Enemy;
    shot.launchTick=tick_;
    shot.launchPosition=attacker->position+
        Vec3(0.f,attacker->height*.55f,0.f);
    shot.destination=target->position+
        Vec3(0.f,target->height*.55f,0.f);
    shot.center=target->position;
    shot.radius=std::max(.4f,profile.collisionRadius);
    const double dist=(shot.destination-shot.launchPosition).lengthXZ();
    shot.flightTicks=std::max<std::uint64_t>(1,
        static_cast<std::uint64_t>(std::ceil(dist*60./double(profile.speed))));
    shot.impactTick=tick_+shot.flightTicks;
    shot.arcHeight=profile.arcHeight;
    shot.visualScale=profile.visualScale;
    shot.tint=profile.tint;
    shot.collisionRadius=profile.collisionRadius;
    shot.collideUnits=profile.collideUnits;
    pendingAreas_.push_back(std::move(shot));
    emitActorCue(hit.attackerId,RTSActorCueEvent::Launch,
                 attacker->position);
    return true;
}
void RTSSkirmish::updateAreaImpacts(){
    // Presentation events have a bounded lifetime and are never authoritative.
    while(!impactFlashes_.empty() &&
          tick_>=impactFlashes_.front().bornTick+FlashTicks)
        impactFlashes_.pop_front();
    // Collect all Weapon impacts before applying them so simultaneous
    // projectile arrivals keep the same two-phase kill semantics as hitscan.
    std::vector<CombatHit> weaponImpacts;
    // Preserve the recorded cast order when multiple projectiles share a tick.
    for(auto it=pendingAreas_.begin();it!=pendingAreas_.end();){
        if(it->projectile)detectProjectileCollision(*it);
        if(it->impactTick>tick_){++it;continue;}
        emitActorCue(it->actorSourceId,RTSActorCueEvent::Impact,it->center);
        if(it->weaponShot){
            const bool landed=it->contactTargetId>0;
            if(landed)weaponImpacts.push_back({
                it->attackerId,it->contactTargetId,it->attackDamage,
                it->attackerName,it->armorPiercing,{}});
            if(impactFlashes_.size()>=MaxImpactFlashes)
                impactFlashes_.pop_front();
            impactFlashes_.push_back({it->center,it->radius,tick_,landed});
        }else{
            const bool applied=applyAreaEffect(*it);
            if(impactFlashes_.size()>=MaxImpactFlashes)
                impactFlashes_.pop_front();
            impactFlashes_.push_back({it->center,it->radius,tick_,applied});
        }
        it=pendingAreas_.erase(it);
    }
    std::sort(weaponImpacts.begin(),weaponImpacts.end(),
        [](const CombatHit& a,const CombatHit& b){
            return a.targetId!=b.targetId?a.targetId<b.targetId:
                   a.attackerId<b.attackerId;
        });
    for(const auto& hit:weaponImpacts)
        if(Unit* unit=find(hit.targetId))
            unit->takeDamage(hit.rawDamage,hit.attackerName,hit.armorPiercing);
}
bool RTSSkirmish::castAbility(int casterId,const std::string& abilityId,
                            int targetId,const Vec3& ground){
    const auto ability=compileAbility(project_.catalog,abilityId);
    if(ability.target==AbilityTarget::Ground){
        if(targetId!=-1 || !canCastAbilityAt(casterId,abilityId,ground))
            return false;
        // Projectiles always enter the future queue, even with a 0-tick
        // post-flight delay. Old instant Areas do not use that queue.
        if((ability.impactDelayTicks>0 || !ability.projectileId.empty()) &&
           pendingAreaImpactCount()>=MaxPendingAreaImpacts)return false;
        const Unit* caster=find(casterId);
        PendingAreaImpact area;
        area.center={ground.x,0.f,ground.z};
        area.radius=ability.areaRadius;
        area.shape=ability.areaShape;
        area.shapeOrigin=caster->position;
        area.areaAngle=ability.areaAngle;
        area.areaWidth=ability.areaWidth;
        area.team=ability.areaTeam;
        area.owner=caster->ownerId;
        area.abilityId=abilityId;
        area.effectId=ability.effect;
        area.actorSourceId=casterId;
        area.impactTick=tick_+ability.impactDelayTicks;
        if(!ability.projectileId.empty()){
            const auto p=compileProjectile(project_.catalog,ability.projectileId);
            area.projectile=true;
            area.projectileId=p.id;
            area.launchTick=tick_;
            area.launchPosition=caster->position+
                Vec3(0.f,caster->height*.66f,0.f);
            area.destination={ground.x,
                project_.terrain.sampleHeight(ground.x,ground.z)+.08f,
                ground.z};
            const double dx=double(area.destination.x)-area.launchPosition.x;
            const double dz=double(area.destination.z)-area.launchPosition.z;
            const double travel=std::sqrt(dx*dx+dz*dz);
            area.flightTicks=std::max<std::uint64_t>(1,
                static_cast<std::uint64_t>(std::ceil(travel*60./double(p.speed))));
            area.impactTick+=area.flightTicks;
            area.arcHeight=p.arcHeight;
            area.visualScale=p.visualScale;
            area.tint=p.tint;
            area.collisionRadius=p.collisionRadius;
            area.collideUnits=p.collideUnits;
            emitActorCue(casterId,RTSActorCueEvent::Launch,
                         caster->position);
        }
        if(ability.impactDelayTicks==0 && !area.projectile){
            if(!applyAreaEffect(area))return false;
            emitActorCue(casterId,RTSActorCueEvent::Impact,area.center);
        }else pendingAreas_.push_back(std::move(area));
    }else{
        if(!canCastAbility(casterId,abilityId,targetId))return false;
        const int appliedTarget=ability.target==AbilityTarget::Self?casterId:targetId;
        if(!applyEffectForTrigger(ability.effect,appliedTarget))return false;
        if(const Unit* target=find(appliedTarget))
            emitActorCue(casterId,RTSActorCueEvent::Impact,target->position);
    }
    playerMinerals-=ability.mineralCost;
    if(Unit* caster=find(casterId))
        caster->currentEnergy=std::max(0.f,
            caster->currentEnergy-float(ability.energyCost));
    abilityReadyAt_[{casterId,abilityId}]=tick_+ability.cooldownTicks;
    if(Unit* caster=find(casterId)){
        caster->attackFlashTimer=std::max(caster->attackFlashTimer,.35f);
        emitActorCue(casterId,RTSActorCueEvent::Cast,caster->position);
    }
    return true;
}
bool RTSSkirmish::queueCommand(RTSCommand command){
    if(result!=MatchResult::Playing || tick_>=RTSCommandStream::MaxCommandTick)return false;
    return commandStream_.enqueue(std::move(command),tick_);
}
bool RTSSkirmish::queuePlayerCommand(RTSCommandKind kind,const Vec3& point,
                                     int targetId,bool append,
                                     const std::string& catalog){
    RTSCommand command;
    command.tick=tick_+1;
    command.kind=kind;
    command.point=point;
    command.targetId=targetId;
    command.append=append;
    command.catalog=catalog;
    for(const auto& u:units)
        if(u.isSelected && u.isAlive() && u.ownerId==0)
            command.units.push_back(u.id);
    std::sort(command.units.begin(),command.units.end());
    command.units.erase(std::unique(command.units.begin(),command.units.end()),
                        command.units.end());
    return queueCommand(std::move(command));
}
bool RTSSkirmish::applyCommand(const RTSCommand& command){
    // Enemy AI uses explicit IDs and a restricted attack command without
    // touching the player's selection or invoking player-only order APIs.
    if(command.source==1){
        if(command.kind!=RTSCommandKind::Attack)return false;
        Unit* target=find(command.targetId);
        if(!target || !target->isAlive() || target->ownerId!=0)return false;
        bool issued=false;
        for(int id:command.units){
            Unit* unit=find(id);
            if(!unit || !unit->isAlive() || unit->isStructure ||
               unit->ownerId!=1)continue;
            routes_.erase(id);queued_.erase(id);jobs_.erase(id);
            unit->attackTarget(command.targetId);
            issued=true;
        }
        return issued;
    }
    // Commands reference identities, never a live selection flag or pointer.
    // Restore visual selection even if execution spawns a new vector element
    // or validation throws, so replays cannot alter UI selection state.
    std::vector<int> previouslySelected;
    for(const auto& unit:units)if(unit.isSelected)previouslySelected.push_back(unit.id);
    std::sort(previouslySelected.begin(),previouslySelected.end());
    auto select=[&](const std::vector<int>& ids){
        for(auto& unit:units)
            unit.isSelected=unit.isAlive() && unit.ownerId==0 &&
                std::binary_search(ids.begin(),ids.end(),unit.id);
    };
    select(command.units);
    const bool previous=applyingCommand_;
    applyingCommand_=true;
    bool result=false;
    try {
        switch(command.kind){
        case RTSCommandKind::Move:
            result=orderMove(command.point,false,command.append);break;
        case RTSCommandKind::AttackMove:
            result=orderMove(command.point,true,command.append);break;
        case RTSCommandKind::Patrol:
            result=orderPatrol(command.point);break;
        case RTSCommandKind::Gather:
            result=orderGather(command.point);break;
        case RTSCommandKind::Attack:
            result=orderAttack(command.targetId);break;
        case RTSCommandKind::Stop:
            orderStop();result=true;break;
        case RTSCommandKind::Hold:
            orderHold();result=true;break;
        case RTSCommandKind::Build:
            result=buildBarracks(command.point);break;
        case RTSCommandKind::Rally:
            result=setRallyPoint(command.point);break;
        case RTSCommandKind::TrainCatalog: {
            const int producer=selectedProducerId();
            result=producer>0 && queueAt(producer,command.catalog);
            break;
        }
        case RTSCommandKind::TrainRanger:
            result=queueRanger();break;
        case RTSCommandKind::TrainWorker:
            result=queueWorker();break;
        case RTSCommandKind::CancelTraining:
            result=cancelSelectedProduction();break;
        case RTSCommandKind::CastAbility:
            for(const int casterId:command.units)
                if(castAbility(casterId,command.catalog,command.targetId,command.point))
                    result=true;
            break;
        case RTSCommandKind::Context:
            result=(command.targetId>=0 && orderAttack(command.targetId)) ||
                   setRallyPoint(command.point) ||
                   orderGather(command.point) ||
                   orderMove(command.point,false,command.append);
            break;
        }
    }catch(...){
        applyingCommand_=previous;
        select(previouslySelected);
        throw;
    }
    applyingCommand_=previous;
    select(previouslySelected);
    return result;
}
std::string RTSSkirmish::stateDigest()const{
    // Canonical integer/quantized serialization; no object pointers, padding,
    // unordered hash iteration or platform-dependent std::hash.
    struct Hash {
        std::uint64_t value=14695981039346656037ULL;
        void number(std::uint64_t n){
            for(int byte=0;byte<8;++byte){
                value^=(n>>(byte*8))&0xffULL;
                value*=1099511628211ULL;
            }
        }
        void signedNumber(std::int64_t n){number(static_cast<std::uint64_t>(n));}
        void boolean(bool b){number(b?1:0);}
        void scalar(double v){
            const double fixed=std::round(v*4096.0);
            signedNumber(std::isfinite(fixed) && std::abs(fixed)<9.0e18
                ?static_cast<std::int64_t>(fixed):0);
        }
        void text(const std::string& s){
            number(s.size());for(unsigned char ch:s){value^=ch;value*=1099511628211ULL;}
        }
        void point(const Vec3& p){scalar(p.x);scalar(p.y);scalar(p.z);}
    } hash;
    hash.number(tick_);
    hash.signedNumber(nextId_);
    hash.signedNumber(friendlyBaseId_);
    hash.signedNumber(enemyBaseId_);
    hash.scalar(elapsedSeconds_);
    hash.scalar(enemySpawnTimer_);
    hash.scalar(visionIntervalTimer_);
    hash.signedNumber(playerMinerals);
    hash.signedNumber(supplyUsed);
    hash.signedNumber(supplyCap);
    hash.signedNumber(static_cast<int>(result));
    hash.number(navigationGeneration_);
    hash.number(navigation.revision());
    for(int z=0;z<navigation.height();++z)for(int x=0;x<navigation.width();++x){
        hash.boolean(navigation.walkable(x,z));
        hash.scalar(navigation.traversalCost(x,z));
    }
    hash.number(minerals.size());
    for(const auto& node:minerals){hash.point(node.position);hash.signedNumber(node.remaining);}
    std::vector<const Unit*> ordered;
    ordered.reserve(units.size());
    for(const auto& unit:units)ordered.push_back(&unit);
    std::sort(ordered.begin(),ordered.end(),
        [](const Unit* a,const Unit* b){return a->id<b->id;});
    hash.number(ordered.size());
    for(const Unit* u:ordered){
        hash.signedNumber(u->id);hash.signedNumber(u->ownerId);
        hash.text(u->catalogId);hash.boolean(u->isStructure);
        hash.boolean(u->isWorker);hash.scalar(u->currentHp);
        if(u->data.max_energy>0.f)hash.scalar(u->currentEnergy);
        hash.point(u->position);hash.scalar(u->facingYawRad);
        hash.signedNumber(static_cast<int>(u->state));
        hash.signedNumber(u->targetUnitId);
        hash.boolean(u->attackMoveEnabled);hash.boolean(u->holdPositionEnabled);
        hash.scalar(u->attackCooldownTimer);hash.scalar(u->attackFlashTimer);
        hash.point(u->navAgent.targetPosition);hash.boolean(u->navAgent.hasDestination);
        hash.point(u->velocity());
    }
    // Only authored ability usage contributes to state hashes, maintaining
    // compatibility with legacy replays on maps without abilities.
    if(!abilityReadyAt_.empty()){
        hash.number(abilityReadyAt_.size());
        for(const auto& cooldown:abilityReadyAt_){
            hash.signedNumber(cooldown.first.first);
            hash.text(cooldown.first.second);
            hash.number(cooldown.second);
        }
    }
    // No Behavior instances means historical command fingerprints are unchanged.
    if(!behaviors_.empty()){
        hash.number(behaviors_.size());
        for(const auto& behavior:behaviors_){
            hash.signedNumber(behavior.first.first);
            hash.text(behavior.first.second);
            hash.signedNumber(behavior.second.stacks);
            hash.number(behavior.second.expiresTick);
        }
    }
    // Future area impacts affect eventual simulation and must be hashed.
    if(!pendingAreas_.empty()){
        hash.text("pending_area_impacts");
        hash.number(pendingAreas_.size());
        for(const auto& area:pendingAreas_){
            hash.point(area.center);hash.scalar(area.radius);
            if(area.shape!=RTSAreaShape::Circle){
                hash.text("shaped_area");
                hash.signedNumber(static_cast<int>(area.shape));
                hash.point(area.shapeOrigin);
                hash.scalar(area.areaAngle);hash.scalar(area.areaWidth);
            }
            hash.signedNumber(area.owner);
            hash.signedNumber(static_cast<int>(area.team));
            hash.text(area.abilityId);hash.text(area.effectId);
            hash.number(area.impactTick);
            if(area.weaponShot){
                hash.text("weapon_direct_hit");
                hash.signedNumber(area.attackerId);
                hash.signedNumber(area.attackTargetId);
                hash.signedNumber(area.contactTargetId);
                hash.scalar(area.attackDamage);
                hash.scalar(area.armorPiercing);
                hash.text(area.attackerName);
            }
            if(area.projectile){
                hash.text("projectile");hash.text(area.projectileId);
                hash.point(area.launchPosition);hash.point(area.destination);
                hash.number(area.launchTick);hash.number(area.flightTicks);
                hash.scalar(area.arcHeight);hash.scalar(area.visualScale);
                hash.point(area.tint);hash.scalar(area.collisionRadius);
                hash.boolean(area.collideUnits);
            }
        }
    }
    // Empty arrays preserve all pre-periodic-effect replay fingerprints.
    // Otherwise include the exact active duration/next pulse state.
    if(!periodicEffects_.empty()){
        hash.number(periodicEffects_.size());
        for(const auto& effect:periodicEffects_){
            hash.signedNumber(effect.targetId);
            hash.signedNumber(static_cast<int>(effect.kind));
            hash.scalar(effect.amount);
            hash.number(effect.intervalTicks);
            hash.number(effect.nextTick);
            hash.number(effect.finalTick);
            hash.text(effect.sourceEffect);
        }
    }
    hash.number(routes_.size());
    for(const auto& item:routes_){
        const auto& r=item.second;
        hash.signedNumber(item.first);hash.number(r.next);hash.point(r.requestedGoal);
        hash.boolean(r.attackMove);hash.boolean(r.patrol);hash.boolean(r.returning);
        hash.boolean(r.engagementInterrupted);hash.point(r.patrolStart);hash.point(r.patrolGoal);
        hash.number(r.navigationGeneration);hash.number(r.gridRevision);
        hash.number(r.points.size());for(const auto& p:r.points)hash.point(p);
    }
    std::vector<int> pendingIds;
    for(const auto& p:queued_)pendingIds.push_back(p.first);
    std::sort(pendingIds.begin(),pendingIds.end());
    hash.number(pendingIds.size());
    for(int id:pendingIds){
        hash.signedNumber(id);const auto& q=queued_.at(id);
        hash.number(q.size());
        for(const auto& move:q){hash.point(move.target);hash.boolean(move.attackMove);}
    }
    hash.number(jobs_.size());
    for(const auto& pair:jobs_){
        hash.signedNumber(pair.first);const auto& job=pair.second;
        hash.number(job.mineralIndex);hash.signedNumber(static_cast<int>(job.phase));
        hash.scalar(job.timer);hash.signedNumber(job.cargo);
    }
    hash.number(production_.size());
    for(const auto& p:production_){
        hash.signedNumber(p.buildingId);hash.text(p.catalogId);
        hash.scalar(p.remaining);hash.signedNumber(p.reservedSupply);
        hash.signedNumber(p.paidMinerals);hash.scalar(p.totalTime);
    }
    std::vector<int> constructionIds;
    for(const auto& p:construction_)constructionIds.push_back(p.first);
    std::sort(constructionIds.begin(),constructionIds.end());
    hash.number(constructionIds.size());
    for(int id:constructionIds){hash.signedNumber(id);hash.scalar(construction_.at(id));}
    std::vector<int> rallyIds;
    for(const auto& p:rallyPoints_)rallyIds.push_back(p.first);
    std::sort(rallyIds.begin(),rallyIds.end());
    hash.number(rallyIds.size());
    for(int id:rallyIds){hash.signedNumber(id);hash.point(rallyPoints_.at(id));}
    hash.number(vision.width());hash.number(vision.depth());
    for(std::uint8_t value:vision.textureBytes())hash.number(value);
    for(const auto& rule:triggers_.rules()){
        hash.text(rule.id);hash.boolean(rule.fired);
        hash.number(rule.executions);hash.number(rule.blocked);
    }
    // Preserve v1/v2 replay hashes for maps with no authored globals.
    // Nonempty variable state is part of the authoritative simulation and
    // must be included in cross-platform fingerprints and replay verification.
    if(!triggers_.variables().empty()) {
        hash.text("trigger_variables");
        hash.number(triggers_.variables().size());
        for(const auto& entry:triggers_.variables()) {
            hash.text(entry.first);
            hash.signedNumber(entry.second);
        }
        // Deferred script events are also part of future authoritative state.
        hash.number(triggers_.pendingVariableChanges().size());
        for(const auto& pending:triggers_.pendingVariableChanges()) {
            hash.text(pending.first);
            hash.signedNumber(pending.second.first);
            hash.signedNumber(pending.second.second);
        }
    }
    hash.text(triggerMessage);
    std::ostringstream s;
    s<<std::hex<<std::setfill('0')<<std::setw(16)<<hash.value;
    return s.str();
}
RTSReplayTape RTSSkirmish::replayTape()const{
    if(!replayValid_ || tick_>RTSReplayTape::MaxTicks)
        throw std::runtime_error("Cannot record replay after untracked editor changes or non-60Hz simulation");
    RTSReplayTape tape;
    tape.project=project_;
    tape.totalTicks=tick_;
    tape.commands=recordedCommands_;
    tape.checkpoints=checkpoints_;
    if(tape.checkpoints.empty() || tape.checkpoints.back().tick!=tick_)
        tape.checkpoints.push_back({tick_,stateDigest()});
    (void)tape.toJson();
    return tape;
}

std::vector<RTSSkirmish::CommandOutcome> RTSSkirmish::takeCommandOutcomes(){
    std::vector<CommandOutcome> result(commandOutcomes_.begin(),commandOutcomes_.end());
    commandOutcomes_.clear();
    return result;
}

void RTSSkirmish::setReplayAIMode(int formatVersion){
    if(formatVersion!=1 && formatVersion!=2)
        throw std::invalid_argument("Unsupported replay AI format");
    aiInputMode_=formatVersion==1?AIInputMode::LegacyDirect:
                                  AIInputMode::ReplayRecorded;
}
