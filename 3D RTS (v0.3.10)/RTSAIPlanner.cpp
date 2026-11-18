#include "RTSAIPlanner.hpp"
#include <algorithm>
#include <cmath>

namespace {
double distSquaredXZ(const Vec3& a,const Vec3& b){
    const double dx=double(a.x)-b.x,dz=double(a.z)-b.z;
    return dx*dx+dz*dz;
}
const Unit* byId(const std::vector<Unit>& units,int id){
    for(const auto& unit:units)if(unit.id==id)return &unit;
    return nullptr;
}
}
std::vector<RTSCommand> planRTSEnemyAttackOrders(
    const std::vector<Unit>& units,int aiOwner,int homeBaseId,
    int strategicObjectiveId,std::uint64_t executionTick,
    const RTSAICombatPolicy& policy){
    std::vector<RTSCommand> planned;
    if(!policy.enabled || aiOwner!=1 || executionTick==0 ||
       executionTick>RTSCommandStream::MaxCommandTick)return planned;
    const Unit* home=byId(units,homeBaseId);
    const Unit* objective=byId(units,strategicObjectiveId);
    if(!home || !home->isAlive() || home->ownerId!=aiOwner ||
       !home->isStructure || !objective || !objective->isAlive() ||
       objective->ownerId==aiOwner)return planned;

    // Only units close to the AI's home base can attract defending units:
    // never scan an omniscient world target table for global aggression.
    const float radius=std::isfinite(policy.defenseRadius)
        ?std::clamp(policy.defenseRadius,0.f,80.f):0.f;
    const double radiusSq=double(radius)*radius;
    std::vector<const Unit*> threats;
    if(radius>0.f)for(const auto& target:units)
        if(target.isAlive() && !target.isStructure &&
           target.ownerId==0 && distSquaredXZ(target.position,home->position)<=radiusSq)
            threats.push_back(&target);
    std::sort(threats.begin(),threats.end(),[](const Unit* a,const Unit* b){
        return a->id<b->id;
    });
    std::vector<const Unit*> idle;
    for(const auto& unit:units)
        if(unit.ownerId==aiOwner && unit.isAlive() && !unit.isStructure &&
           unit.state==UnitState::IDLE)idle.push_back(&unit);
    std::sort(idle.begin(),idle.end(),[](const Unit* a,const Unit* b){
        return a->id<b->id;
    });
    // A cheap O(idle * nearby-threats) pass. No RNG, clocks, container-order
    // dependence, hidden mutating callbacks, or unbounded global path searches.
    planned.reserve(idle.size());
    for(const auto* actor:idle){
        int targetId=objective->id;
        double best=0.;
        for(const Unit* threat:threats){
            const double d=distSquaredXZ(actor->position,threat->position);
            if(targetId==objective->id || d<best ||
               (d==best && threat->id<targetId)){
                targetId=threat->id;best=d;
            }
        }
        RTSCommand command;
        command.tick=executionTick;
        command.source=1;
        command.kind=RTSCommandKind::Attack;
        command.units={actor->id};
        command.targetId=targetId;
        planned.push_back(std::move(command));
    }
    return planned;
}

std::vector<RTSCommand> planRTSEnemyResearchOrders(
    const std::vector<RTSAIResearchCandidate>& legalCandidates,
    const std::vector<std::string>& upgradePriorities,
    std::uint64_t executionTick){
    if(executionTick==0 || executionTick>RTSCommandStream::MaxCommandTick ||
       upgradePriorities.empty())return {};
    // Priority order is author-controlled. Repeated candidates for the
    // same Upgrade resolve by lowest stable producer ID, not unit vector
    // iteration order, hash-map iteration or wall-clock/RNG decisions.
    for(const std::string& id:upgradePriorities){
        if(id.empty())continue;
        const RTSAIResearchCandidate* best=nullptr;
        for(const auto& candidate:legalCandidates){
            if(candidate.upgradeId!=id || candidate.producerId<=0 ||
               candidate.nextLevel<=0 || candidate.mineralCost<0)continue;
            if(!best || candidate.producerId<best->producerId)best=&candidate;
        }
        if(!best)continue;
        RTSCommand command;
        command.tick=executionTick;
        command.source=1;
        command.kind=RTSCommandKind::ResearchUpgrade;
        command.units={best->producerId};
        command.catalog=id;
        return {command};
    }
    return {};
}
