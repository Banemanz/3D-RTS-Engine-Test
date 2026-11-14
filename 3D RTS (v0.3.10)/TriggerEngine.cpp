#include "TriggerEngine.hpp"
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
    std::vector<Rule> result;
    auto it=project.extensions.find("triggers");
    if(it==project.extensions.end())return result;
    if(!it->second.is<picojson::array>())
        throw std::invalid_argument("triggers must be a JSON array");
    const auto& definitions=it->second.get<picojson::array>();
    if(definitions.size()>256)throw std::invalid_argument("At most 256 triggers per project");
    std::set<std::string> ids;
    for(const auto& val:definitions) {
        if(!val.is<picojson::object>())throw std::invalid_argument("Trigger must be an object");
        const auto& o=val.get<picojson::object>();
        Rule rule;
        rule.id=str(o,"id");
        if(rule.id.empty() || rule.id.size()>128 || !ids.insert(rule.id).second)
            throw std::invalid_argument("Invalid or duplicate trigger ID");
        auto event=o.find("event");
        if(event==o.end() || !event->second.is<picojson::object>())
            throw std::invalid_argument("Trigger event must be object");
        const auto& e=event->second.get<picojson::object>();
        rule.event=str(e,"type");
        requireType(rule.event,{"OnMapStart","After","Every","OnUnitDeath","OnRegionEnter","MineralsBelow"});
        rule.region=str(e,"region");
        if(rule.event=="OnRegionEnter") {
            bool found=false;
            for(const auto& p:project.placements)
                if(p.id==rule.region && p.kind=="Region")found=true;
            if(!found)throw std::invalid_argument("Trigger missing Region placement: "+rule.region);
        }
        if(rule.event=="After" || rule.event=="Every")
            rule.interval=num(e,"seconds");
        if(rule.event=="OnUnitDeath") {
            // Optional filters should be provided through SubjectOwnerIs condition.
        }
        if(rule.event=="MineralsBelow") {
            rule.threshold=num(e,"amount",-1);
            if(rule.threshold<0 || rule.threshold>100000000)
                throw std::invalid_argument("MineralsBelow requires valid amount");
        }
        if(rule.event=="Every" && (rule.interval<.1 || rule.interval>360000))
            throw std::invalid_argument("Periodic interval must be 0.1..360000 seconds");
        if(rule.event=="After" && (rule.interval<0 || rule.interval>360000))
            throw std::invalid_argument("Delayed time must be 0..360000 seconds");
        rule.once=flag(o,"once",rule.event=="OnMapStart" || rule.event=="After");
        auto cond=o.find("conditions");
        if(cond!=o.end()) {
            if(!cond->second.is<picojson::array>() ||
               cond->second.get<picojson::array>().size()>32)
                throw std::invalid_argument("Invalid trigger conditions");
            for(const auto& entry:cond->second.get<picojson::array>()) {
                if(!entry.is<picojson::object>())throw std::invalid_argument("Invalid trigger condition");
                const auto& ob=entry.get<picojson::object>();
                auto type=str(ob,"type");
                requireType(type,{"PlayerMineralsAtLeast","UnitCountAtLeast",
                                  "UnitTypeCountAtLeast","SubjectOwnerIs"});
                if(type=="PlayerMineralsAtLeast")
                    (void)intField(ob,"amount",0,0,100000000);
                if(type=="UnitCountAtLeast" || type=="UnitTypeCountAtLeast") {
                    (void)intField(ob,"owner",0,-1,15);
                    (void)intField(ob,"count",1,0,4000);
                    if(type=="UnitTypeCountAtLeast") {
                        const std::string id=str(ob,"catalog");
                        const auto* entry=project.catalog.find(id);
                        if(!entry || entry->type!="Unit")
                            throw std::invalid_argument("Missing unit catalog for condition");
                    }
                }
                if(type=="SubjectOwnerIs")
                    (void)intField(ob,"owner",0,0,15);
                rule.conditions.push_back(ob);
            }
        }
        auto actions=o.find("actions");
        if(actions==o.end() || !actions->second.is<picojson::array>() ||
           actions->second.get<picojson::array>().empty() ||
           actions->second.get<picojson::array>().size()>64)
            throw std::invalid_argument("Trigger requires 1..64 actions");
        for(const auto& entry:actions->second.get<picojson::array>()) {
            if(!entry.is<picojson::object>())throw std::invalid_argument("Trigger action must be object");
            auto ob=entry.get<picojson::object>();
            auto type=str(ob,"type");
            requireType(type,{"GrantMinerals","SpawnUnit","EndMatch","ShowMessage",
                              "ApplyEffect"});
            if(type=="SpawnUnit") {
                auto catalog=str(ob,"catalog");
                auto unit=project.catalog.find(catalog);
                if(!unit || unit->type!="Unit")
                    throw std::invalid_argument("Trigger spawn catalog is not a Unit: "+catalog);
                (void)intField(ob,"owner",1,0,15);
                const double x=num(ob,"x"),z=num(ob,"z");
                if(!project.terrain.insideWorld(float(x),float(z)))
                    throw std::invalid_argument("Trigger spawn is outside map bounds");
            }
            if(type=="ApplyEffect"){
                if(rule.event!="OnRegionEnter")
                    throw std::invalid_argument("ApplyEffect requires OnRegionEnter subject");
                const auto effect=str(ob,"effect");
                const auto* object=project.catalog.find(effect);
                if(!object || object->type!="Effect")
                    throw std::invalid_argument("ApplyEffect references missing Effect: "+effect);
                (void)compileEffect(project.catalog,effect);
            }
            if(type=="GrantMinerals")
                (void)intField(ob,"amount",0,-100000000,100000000);
            if(type=="EndMatch") {
                const auto result=str(ob,"result","Victory");
                if(result!="Victory" && result!="Defeat")
                    throw std::invalid_argument("Trigger must end Victory or Defeat");
            }
            if(type=="ShowMessage" && str(ob,"text").size()>256)
                throw std::invalid_argument("Trigger message exceeds 256 bytes");
            rule.actions.push_back(std::move(ob));
        }
        result.push_back(std::move(rule));
    }
    return result;
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
