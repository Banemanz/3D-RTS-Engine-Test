#include "TriggerDefinition.hpp"
#include "RTSProject.hpp"
#include "RTSScriptExpression.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
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

// Script state is a typed, bounded project resource, not arbitrary code.
// Integer semantics avoid cross-compiler floating point and replay ambiguity.
std::map<std::string,int> parseTriggerVariables(const RTSProject& project) {
    std::map<std::string,int> result;
    const auto it=project.extensions.find("trigger_variables");
    if(it==project.extensions.end())return result;
    if(!it->second.is<picojson::object>())
        throw std::invalid_argument("trigger_variables must be an object");
    const auto& globals=it->second.get<picojson::object>();
    if(globals.size()>64)throw std::invalid_argument("At most 64 trigger variables");
    for(const auto& entry:globals) {
        const std::string& name=entry.first;
        if(name.empty() || name.size()>64 ||
           !(name[0]=='_' || (name[0]>='A' && name[0]<='Z') ||
             (name[0]>='a' && name[0]<='z')))
            throw std::invalid_argument("Invalid trigger variable name: "+name);
        for(char ch:name)if(!(ch=='_' || (ch>='A' && ch<='Z') ||
                               (ch>='a' && ch<='z') || (ch>='0' && ch<='9')))
            throw std::invalid_argument("Invalid trigger variable name: "+name);
        if(!entry.second.is<double>())
            throw std::invalid_argument("Trigger variable must be integer: "+name);
        const double value=entry.second.get<double>();
        if(!std::isfinite(value) || std::floor(value)!=value ||
           value<-100000000. || value>100000000.)
            throw std::invalid_argument("Trigger variable out of range: "+name);
        result.emplace(name,static_cast<int>(value));
    }
    return result;
}

std::vector<TriggerEngine::Rule> parseTriggerRules(const RTSProject& project) {
    const auto variables=parseTriggerVariables(project);
    auto requireVariable=[&](const picojson::object& node) {
        const auto name=str(node,"variable");
        if(!variables.count(name))
            throw std::invalid_argument("Trigger references missing variable: "+name);
    };
    std::vector<TriggerEngine::Rule> result;
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
        TriggerEngine::Rule rule;
        rule.id=str(o,"id");
        if(rule.id.empty() || rule.id.size()>128 || !ids.insert(rule.id).second)
            throw std::invalid_argument("Invalid or duplicate trigger ID");
        auto event=o.find("event");
        if(event==o.end() || !event->second.is<picojson::object>())
            throw std::invalid_argument("Trigger event must be object");
        const auto& e=event->second.get<picojson::object>();
        rule.event=str(e,"type");
        requireType(rule.event,{"OnMapStart","After","Every","OnUnitDeath","OnRegionEnter","MineralsBelow","OnVariableChanged"});
        rule.region=str(e,"region");
        if(rule.event=="OnVariableChanged") {
            rule.variable=str(e,"variable");
            if(!variables.count(rule.variable))
                throw std::invalid_argument("Variable event references missing variable: "+rule.variable);
        }
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
        // Shared validation for top-level rules and nested control-flow nodes.
        const auto validateCondition=[&](const picojson::value& value){
            if(!value.is<picojson::object>())
                throw std::invalid_argument("Invalid trigger condition");
            const auto& ob=value.get<picojson::object>();
            const auto type=str(ob,"type");
            requireType(type,{"PlayerMineralsAtLeast","UnitCountAtLeast",
                              "UnitTypeCountAtLeast","SubjectOwnerIs","VariableCompare"});
            if(type=="PlayerMineralsAtLeast")
                (void)intField(ob,"amount",0,0,100000000);
            if(type=="UnitCountAtLeast" || type=="UnitTypeCountAtLeast"){
                (void)intField(ob,"owner",0,-1,15);
                (void)intField(ob,"count",1,0,4000);
                if(type=="UnitTypeCountAtLeast"){
                    const auto id=str(ob,"catalog");
                    const auto* catalog=project.catalog.find(id);
                    if(!catalog || catalog->type!="Unit")
                        throw std::invalid_argument("Missing Unit catalog for condition: "+id);
                }
            }
            if(type=="SubjectOwnerIs")
                (void)intField(ob,"owner",0,0,15);
            if(type=="VariableCompare"){
                requireVariable(ob);
                requireType(str(ob,"operator"),{"==","!=","<","<=",">",">="});
                const auto expr=ob.find("value_expr");
                if(expr!=ob.end()){
                    if(ob.count("value"))
                        throw std::invalid_argument("Ambiguous VariableCompare operand");
                    RTSScriptExpression::validate(expr->second,variables);
                }else (void)intField(ob,"value",0,-100000000,100000000);
            }
        };
        const auto conditions=o.find("conditions");
        if(conditions!=o.end()){
            if(!conditions->second.is<picojson::array>() ||
               conditions->second.get<picojson::array>().size()>32)
                throw std::invalid_argument("Invalid trigger conditions");
            for(const auto& condition:conditions->second.get<picojson::array>()){
                validateCondition(condition);
                rule.conditions.push_back(condition.get<picojson::object>());
            }
        }
        const auto actions=o.find("actions");
        if(actions==o.end() || !actions->second.is<picojson::array>() ||
           actions->second.get<picojson::array>().empty() ||
           actions->second.get<picojson::array>().size()>64)
            throw std::invalid_argument("Trigger requires 1..64 actions");

        std::size_t totalNodes=0;
        std::function<void(const picojson::array&,int)> validateActions;
        validateActions=[&](const picojson::array& nodes,int depth){
            if(depth>4)throw std::invalid_argument("Trigger control flow exceeds depth 4");
            for(const auto& node:nodes){
                if(++totalNodes>256)
                    throw std::invalid_argument("Trigger action graph exceeds 256 nodes");
                if(!node.is<picojson::object>())
                    throw std::invalid_argument("Trigger action must be an object");
                const auto& ob=node.get<picojson::object>();
                const auto type=str(ob,"type");
                requireType(type,{"GrantMinerals","SpawnUnit","EndMatch","ShowMessage",
                                  "ApplyEffect","SetVariable","AddVariable",
                                  "IfThenElse","Repeat"});
                if(type=="IfThenElse"){
                    const auto ci=ob.find("conditions");
                    const auto ti=ob.find("then");
                    const auto ei=ob.find("else");
                    if(ci==ob.end() || !ci->second.is<picojson::array>() ||
                       ci->second.get<picojson::array>().empty() ||
                       ci->second.get<picojson::array>().size()>16 ||
                       ti==ob.end() || !ti->second.is<picojson::array>() ||
                       ti->second.get<picojson::array>().empty() ||
                       ti->second.get<picojson::array>().size()>32 ||
                       (ei!=ob.end() && (!ei->second.is<picojson::array>() ||
                                          ei->second.get<picojson::array>().size()>32)))
                        throw std::invalid_argument("Invalid IfThenElse branches");
                    for(const auto& condition:ci->second.get<picojson::array>())
                        validateCondition(condition);
                    validateActions(ti->second.get<picojson::array>(),depth+1);
                    if(ei!=ob.end())validateActions(ei->second.get<picojson::array>(),depth+1);
                    continue;
                }
                if(type=="Repeat"){
                    const auto count=ob.find("times_expr");
                    if(count!=ob.end()){
                        if(ob.count("times"))
                            throw std::invalid_argument("Repeat has ambiguous loop count");
                        RTSScriptExpression::validate(count->second,variables);
                    }else (void)intField(ob,"times",1,0,16);
                    const auto body=ob.find("actions");
                    if(body==ob.end() || !body->second.is<picojson::array>() ||
                       body->second.get<picojson::array>().empty() ||
                       body->second.get<picojson::array>().size()>32)
                        throw std::invalid_argument("Repeat requires 1..32 body actions");
                    validateActions(body->second.get<picojson::array>(),depth+1);
                    continue;
                }
                if(type=="SpawnUnit"){
                    const auto id=str(ob,"catalog");
                    const auto* unit=project.catalog.find(id);
                    if(!unit || unit->type!="Unit")
                        throw std::invalid_argument("Trigger spawn is not a Unit: "+id);
                    (void)intField(ob,"owner",1,0,15);
                    const double x=num(ob,"x"),z=num(ob,"z");
                    if(!project.terrain.insideWorld(float(x),float(z)))
                        throw std::invalid_argument("Trigger spawn is outside map bounds");
                }
                if(type=="ApplyEffect"){
                    if(rule.event!="OnRegionEnter")
                        throw std::invalid_argument("ApplyEffect requires OnRegionEnter subject");
                    const auto effect=str(ob,"effect");
                    const auto* catalog=project.catalog.find(effect);
                    if(!catalog || catalog->type!="Effect")
                        throw std::invalid_argument("Missing Effect: "+effect);
                    (void)compileEffect(project.catalog,effect);
                }
                if(type=="GrantMinerals"){
                    const auto expr=ob.find("amount_expr");
                    if(expr!=ob.end()){
                        if(ob.count("amount"))
                            throw std::invalid_argument("Ambiguous GrantMinerals operand");
                        RTSScriptExpression::validate(expr->second,variables);
                    }else (void)intField(ob,"amount",0,-100000000,100000000);
                }
                if(type=="SetVariable" || type=="AddVariable"){
                    requireVariable(ob);
                    const auto expr=ob.find("value_expr");
                    if(expr!=ob.end()){
                        if(ob.count("value"))
                            throw std::invalid_argument("Ambiguous variable-action operand");
                        RTSScriptExpression::validate(expr->second,variables);
                    }else (void)intField(ob,"value",0,-100000000,100000000);
                }
                if(type=="EndMatch")
                    requireType(str(ob,"result","Victory"),{"Victory","Defeat"});
                if(type=="ShowMessage" && str(ob,"text").size()>256)
                    throw std::invalid_argument("Trigger message exceeds 256 bytes");
            }
        };
        validateActions(actions->second.get<picojson::array>(),0);
        for(const auto& node:actions->second.get<picojson::array>())
            rule.actions.push_back(node.get<picojson::object>());
        result.push_back(std::move(rule));
    }
    return result;
}
