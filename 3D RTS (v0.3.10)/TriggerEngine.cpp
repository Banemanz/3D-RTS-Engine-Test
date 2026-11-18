#include "TriggerEngine.hpp"
#include "TriggerDefinition.hpp"
#include "RTSSkirmish.hpp"
#include "RTSScriptExpression.hpp"
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
int scriptOperand(const picojson::object& action,const char* literalKey,
                  const char* expressionKey,int fallback,
                  const std::map<std::string,int>& variables){
    const auto expression=action.find(expressionKey);
    if(expression!=action.end())
        return RTSScriptExpression::evaluate(expression->second,variables);
    return intField(action,literalKey,fallback,
                    -RTSScriptExpression::MaxValue,RTSScriptExpression::MaxValue);
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
void TriggerEngine::configure(const RTSProject& p){
    auto next=parse(p);
    auto initialVariables=parseTriggerVariables(p);
    rules_=std::move(next);
    variables_=std::move(initialVariables);
    pendingVariableChanges_.clear();
    pendingUIFrameClicks_.clear();
    trace_.clear();
    nextSequence_=1;
    lastSimulationSeconds_=0.;
}
bool TriggerEngine::conditionsPass(const Rule& rule,const RTSSkirmish& game,
                                  int subjectId,std::size_t* failedCondition)const{
    for(std::size_t i=0;i<rule.conditions.size();++i) {
        const auto& condition=rule.conditions[i];
        const std::string type=str(condition,"type");
        bool failed=false;
        if(type=="PlayerMineralsAtLeast")
            failed=game.playerMinerals<num(condition,"amount");
        else if(type=="UnitCountAtLeast")
            failed=game.countUnitsForTrigger(intField(condition,"owner",0,-1,15),
                                             str(condition,"catalog"))<num(condition,"count",1);
        else if(type=="UnitTypeCountAtLeast")
            failed=game.countUnitsForTrigger(intField(condition,"owner",0,-1,15),
                                             str(condition,"catalog"))<num(condition,"count",1);
        else if(type=="SubjectOwnerIs")
            failed=subjectId<0 ||
                   game.ownerForTrigger(subjectId)!=intField(condition,"owner",0,0,15);
        else if(type=="VariableCompare"){
            const int current=variables_.at(str(condition,"variable"));
            const int reference=scriptOperand(condition,"value","value_expr",0,variables_);
            const auto comparison=str(condition,"operator");
            if(comparison=="==")failed=current!=reference;
            else if(comparison=="!=")failed=current==reference;
            else if(comparison=="<")failed=!(current<reference);
            else if(comparison=="<=")failed=!(current<=reference);
            else if(comparison==">")failed=!(current>reference);
            else if(comparison==">=")failed=!(current>=reference);
        }
        if(failed){
            if(failedCondition)*failedCondition=i;
            return false;
        }
    }
    return true;
}
void TriggerEngine::record(const Rule& rule,double elapsed,int subjectId,
                           TraceStatus status,const std::string& detail){
    if(!tracingEnabled_)return; // zero allocations unless tracing is enabled
    if(trace_.size()==TraceCapacity)trace_.pop_front();
    trace_.push_back({nextSequence_++,rule.id,rule.event,detail,elapsed,subjectId,status});
}
void TriggerEngine::executeActions(const std::vector<picojson::object>& actions,
                                   RTSSkirmish& game,int subjectId,
                                   std::string& summary,std::size_t& steps,int depth){
    if(depth>4)throw std::runtime_error("Trigger action recursion limit exceeded");
    const auto unpack=[](const picojson::array& values){
        std::vector<picojson::object> result;
        result.reserve(values.size());
        for(const auto& value:values)result.push_back(value.get<picojson::object>());
        return result;
    };
    for(const auto& action:actions){
        if(++steps>512)throw std::runtime_error("Trigger action budget exceeded");
        const std::string type=str(action,"type");
        if(type=="IfThenElse"){
            Rule branch;
            for(const auto& cond:action.at("conditions").get<picojson::array>())
                branch.conditions.push_back(cond.get<picojson::object>());
            const bool selected=conditionsPass(branch,game,subjectId,nullptr);
            const char* key=selected?"then":"else";
            auto part=action.find(key);
            if(part!=action.end())
                executeActions(unpack(part->second.get<picojson::array>()),
                               game,subjectId,summary,steps,depth+1);
        }else if(type=="Repeat"){
            const int count=action.count("times_expr")
                ?RTSScriptExpression::evaluate(action.at("times_expr"),variables_)
                :intField(action,"times",1,0,16);
            const auto body=unpack(action.at("actions").get<picojson::array>());
            for(int i=0;i<std::clamp(count,0,16);++i)
                executeActions(body,game,subjectId,summary,steps,depth+1);
        }else if(type=="GrantMinerals")
            game.addMineralsForTrigger(scriptOperand(
                action,"amount","amount_expr",0,variables_));
        else if(type=="SpawnUnit")
            game.spawnForTrigger(str(action,"catalog"),
                intField(action,"owner",1,0,15),
                Vec3(float(num(action,"x")),0.f,float(num(action,"z"))));
        else if(type=="EndMatch")
            game.endMatchForTrigger(str(action,"result","Victory")=="Victory");
        else if(type=="ShowMessage")
            game.showMessageForTrigger(str(action,"text"));
        else if(type=="ApplyEffect")
            game.applyEffectForTrigger(str(action,"effect"),subjectId);
        else if(type=="SetVariable" || type=="AddVariable"){
            const std::string name=str(action,"variable");
            const int value=scriptOperand(action,"value","value_expr",0,variables_);
            int& current=variables_.at(name);
            const int prior=current;
            const std::int64_t next=type=="SetVariable"?std::int64_t(value):
                                    std::int64_t(current)+value;
            current=static_cast<int>(std::clamp(next,std::int64_t(-100000000),
                                                 std::int64_t(100000000)));
            if(current!=prior){
                auto [it,created]=pendingVariableChanges_.emplace(
                    name,std::make_pair(prior,current));
                if(!created)it->second.second=current;
            }
        }
        if(tracingEnabled_){
            if(!summary.empty())summary+=", ";
            summary+=type;
        }
    }
}
void TriggerEngine::run(Rule& rule,RTSSkirmish& game,int subjectId,double elapsed) {
    if(rule.once && rule.fired)return;
    ++rule.attempts;
    std::size_t failedIndex=0;
    if(!conditionsPass(rule,game,subjectId,&failedIndex)){
        ++rule.blocked;
        if(tracingEnabled_){
            const auto conditionType=str(rule.conditions[failedIndex],"type");
            record(rule,elapsed,subjectId,TraceStatus::ConditionBlocked,
                   "Condition "+std::to_string(failedIndex+1)+" failed: "+conditionType);
        }
        return;
    }
    rule.fired=true;
    std::string summary;
    std::size_t steps=0;
    try{
        executeActions(rule.actions,game,subjectId,summary,steps,0);
    }catch(const std::exception& e){
        record(rule,elapsed,subjectId,TraceStatus::ActionError,e.what());
        throw;
    }
    ++rule.executions;
    record(rule,elapsed,subjectId,TraceStatus::Executed,summary);
}
bool TriggerEngine::queueUIFrameClick(const std::string& frameId){
    if(frameId.empty() || pendingUIFrameClicks_.size()>=MaxUIFrameClicksPerTick)
        return false;
    pendingUIFrameClicks_.push_back(frameId);
    return true;
}
void TriggerEngine::start(RTSSkirmish& game) {
    lastSimulationSeconds_=0.;
    for(auto& r:rules_)if(r.event=="OnMapStart")run(r,game,-1,0.);
}
void TriggerEngine::tick(RTSSkirmish& game,double elapsed,
                         const std::vector<int>& killed) {
    lastSimulationSeconds_=elapsed;
    // Recorded player commands are applied before this tick's triggers.
    // Deliver UI events in command sequence order with a bounded budget.
    // Each rule still observes its own Once flag, normal conditions,
    // action error tracing and the standard ECA execution sandbox.
    auto clicks=std::move(pendingUIFrameClicks_);
    pendingUIFrameClicks_.clear();
    for(const auto& frame:clicks)
        for(auto& rule:rules_)
            if(rule.event=="OnUIFrameClicked" && rule.uiFrame==frame)
                run(rule,game,-1,elapsed);
    // Deliver a snapshot of the prior tick's changes; callbacks cannot
    // reenter this batch. Coalesce each variable's first/last values to
    // guarantee bounded work (at most 64 globals, 256 authored rules).
    auto changes=std::move(pendingVariableChanges_);
    pendingVariableChanges_.clear();
    for(auto& r:rules_) {
        if(r.once && r.fired)continue;
        if(r.event=="OnVariableChanged"){
            const auto changed=changes.find(r.variable);
            if(changed!=changes.end() && changed->second.first!=changed->second.second)
                run(r,game,-1,elapsed);
        }else if(r.event=="After" && elapsed>=r.interval)run(r,game,-1,elapsed);
        else if(r.event=="Every" && elapsed>=r.startTime+r.interval) {
            r.startTime=elapsed;
            run(r,game,-1,elapsed);
        }else if(r.event=="MineralsBelow") {
            if(game.playerMinerals<r.threshold)run(r,game,-1,elapsed);
        }else if(r.event=="OnUnitDeath") {
            for(int id:killed)run(r,game,id,elapsed);
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
            for(int id:entered)run(r,game,id,elapsed);
        }
    }
}
