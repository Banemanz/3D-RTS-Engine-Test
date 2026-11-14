#include "TriggerEngine.hpp"
#include "TriggerDefinition.hpp"
#include "RTSSkirmish.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
std::string str(const picojson::object& o,const char* key,const std::string& def="") {
    auto i=o.find(key);if(i==o.end())return def;
    if(!i->second.is<std::string>())
        throw std::invalid_argument(std::string("Trigger '")+key+"' must be string");
    return i->second.get<std::string>();
}
double num(const picojson::object& o,const char* key,double def=0) {
    auto i=o.find(key);if(i==o.end())return def;
    if(!i->second.is<double>() || !std::isfinite(i->second.get<double>()))
        throw std::invalid_argument(std::string("Trigger '")+key+"' must be finite number");
    return i->second.get<double>();
}
int intField(const picojson::object& o,const char* key,int fallback,int low,int high) {
    auto i=o.find(key);
    if(i==o.end())return fallback;
    if(!i->second.is<double>())throw std::invalid_argument(std::string("Trigger integer field: ")+key);
    const double v=i->second.get<double>();
    if(!std::isfinite(v) || std::floor(v)!=v || v<double(low) || v>double(high))
        throw std::invalid_argument(std::string("Trigger integer out of range: ")+key);
    return static_cast<int>(v);
}
bool flag(const picojson::object& o,const char* key,bool def=false) {
    auto i=o.find(key);if(i==o.end())return def;
    if(!i->second.is<bool>())throw std::invalid_argument("Trigger bool field invalid");
    return i->second.get<bool>();
}
void requireType(const std::string& type,const std::set<std::string>& allowed) {
    if(allowed.count(type)==0)throw std::invalid_argument("Unknown trigger type: "+type);
}
}
std::vector<TriggerEngine::Rule> TriggerEngine::parse(const RTSProject& project)const {
    return parseTriggerRules(project);
}
void TriggerEngine::validate(const RTSProject& p){ TriggerEngine e;(void)e.parse(p); }
void TriggerEngine::configure(const RTSProject& p){rules_=parse(p);}
bool TriggerEngine::conditionsPass(const Rule& rule,const RTSSkirmish& game,int subjectId)const {
    for(const auto& condition:rule.conditions) {
        std::string type=str(condition,"type");
        if(type=="PlayerMineralsAtLeast" && game.playerMinerals<num(condition,"amount"))
            return false;
        if(type=="UnitCountAtLeast" &&
           game.countUnitsForTrigger(intField(condition,"owner",0,-1,15),
                                     str(condition,"catalog"))<num(condition,"count",1))
            return false;
        if(type=="UnitTypeCountAtLeast" &&
           game.countUnitsForTrigger(int(num(condition,"owner",0)),
                                     str(condition,"catalog"))<num(condition,"count",1))
            return false;
        if(type=="SubjectOwnerIs" &&
           (subjectId<0 || game.ownerForTrigger(subjectId)!=intField(condition,"owner",0,0,15)))
            return false;
    }
    return true;
}
void TriggerEngine::run(Rule& rule,RTSSkirmish& game,int subjectId) {
    if(rule.once && rule.fired)return;
    if(!conditionsPass(rule,game,subjectId))return;
    rule.fired=true;
    for(const auto& action:rule.actions) {
        const std::string type=str(action,"type");
        if(type=="GrantMinerals")
            game.addMineralsForTrigger(intField(action,"amount",0,-100000000,100000000));
        else if(type=="SpawnUnit")
            game.spawnForTrigger(str(action,"catalog"),
                                intField(action,"owner",1,0,15),
                                Vec3(float(num(action,"x")),0,float(num(action,"z"))));
        else if(type=="EndMatch") {
            auto outcome=str(action,"result","Victory");
            if(outcome!="Victory" && outcome!="Defeat")
                throw std::invalid_argument("EndMatch result must be Victory or Defeat");
            game.endMatchForTrigger(outcome=="Victory");
        }else if(type=="ShowMessage")
            game.showMessageForTrigger(str(action,"text"));
        else if(type=="ApplyEffect")
            game.applyEffectForTrigger(str(action,"effect"),subjectId);
    }
}
void TriggerEngine::start(RTSSkirmish& game) {
    for(auto& r:rules_)if(r.event=="OnMapStart")run(r,game,-1);
}
void TriggerEngine::tick(RTSSkirmish& game,double elapsed,
                         const std::vector<int>& killed) {
    for(auto& r:rules_) {
        if(r.once && r.fired)continue;
        if(r.event=="After" && elapsed>=r.interval)run(r,game,-1);
        else if(r.event=="Every" && elapsed>=r.startTime+r.interval) {
            r.startTime=elapsed;
            run(r,game,-1);
        }else if(r.event=="MineralsBelow") {
            if(game.playerMinerals<r.threshold)run(r,game,-1);
        }else if(r.event=="OnUnitDeath") {
            for(int id:killed)run(r,game,id);
        }else if(r.event=="OnRegionEnter") {
            const auto* region=game.regionForTrigger(r.region);
            if(!region)continue;
            double radius=3.0;
            auto i=region->overrides.find("radius");
            if(i!=region->overrides.end() && i->second.is<double>())
                radius=std::clamp(i->second.get<double>(),.1,512.0);
            std::unordered_set<int> current;
            std::vector<int> entered;
            for(const auto& unit:game.units) {
                if(!unit.isAlive() || unit.isStructure)continue;
                if((unit.position-region->position).lengthXZ()<=radius) {
                    current.insert(unit.id);
                    if(!r.occupants.count(unit.id))entered.push_back(unit.id);
                }
            }
            r.occupants=std::move(current);
            // A SpawnUnit action may reallocate game.units; never execute it
            // while walking that vector.
            for(int id:entered)run(r,game,id);
        }
    }
}
