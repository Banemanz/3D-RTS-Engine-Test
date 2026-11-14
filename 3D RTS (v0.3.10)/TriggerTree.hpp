#pragma once
#include "RTSProject.hpp"
#include "RTSScriptExpression.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

// Stable authoring address within ONE selected rule. Every step identifies an
// array and its item; intermediate containers must be recognized control-flow
// nodes. Never retain pointers into a project across undo/reload.
struct TriggerTreeStep {
    std::string list;
    std::size_t index=0;
};
using TriggerTreePath=std::vector<TriggerTreeStep>;

namespace TriggerTree {
inline const std::string& typeOf(const picojson::object& object){
    auto it=object.find("type");
    if(it==object.end() || !it->second.is<std::string>())
        throw std::invalid_argument("Trigger node has no type");
    return it->second.get<std::string>();
}
inline bool allowed(const picojson::object& parent,const std::string& list,
                    bool root){
    if(root)return list=="actions" || list=="conditions";
    const std::string& type=typeOf(parent);
    return (type=="IfThenElse" &&
            (list=="conditions" || list=="then" || list=="else")) ||
           (type=="Repeat" && list=="actions");
}
inline picojson::object& objectAt(picojson::object& root,
                                 const TriggerTreePath& path){
    if(path.empty() || path.size()>7)
        throw std::invalid_argument("Invalid trigger node address");
    picojson::object* current=&root;
    for(std::size_t depth=0;depth<path.size();++depth){
        const auto& step=path[depth];
        if(!allowed(*current,step.list,depth==0))
            throw std::invalid_argument("Invalid trigger tree container");
        const auto found=current->find(step.list);
        if(found==current->end() || !found->second.is<picojson::array>())
            throw std::invalid_argument("Trigger tree list does not exist");
        auto& children=found->second.get<picojson::array>();
        if(step.index>=children.size() || !children[step.index].is<picojson::object>())
            throw std::invalid_argument("Trigger tree node index out of range");
        current=&children[step.index].get<picojson::object>();
    }
    return *current;
}
inline picojson::array& listAt(picojson::object& root,
                              const TriggerTreePath& parent,
                              const std::string& list){
    picojson::object& object=parent.empty()?root:objectAt(root,parent);
    if(!allowed(object,list,parent.empty()))
        throw std::invalid_argument("Invalid nested trigger list");
    auto it=object.find(list);
    // Optional 'else' and top-level conditions are materialized on insert.
    if(it==object.end() && (list=="else" ||
                            (parent.empty() && list=="conditions")))
        it=object.emplace(list,picojson::value(picojson::array{})).first;
    if(it==object.end() || !it->second.is<picojson::array>())
        throw std::invalid_argument("Nested trigger list does not exist");
    return it->second.get<picojson::array>();
}
inline picojson::array listSnapshot(const picojson::object& root,
                                   const TriggerTreePath& parent,
                                   const std::string& list){
    picojson::object copy=root;
    return listAt(copy,parent,list);
}
inline picojson::value& expressionAt(picojson::object& node,
                                    const std::string& key,
                                    const std::vector<std::string>& path){
    if(key!="value_expr" && key!="amount_expr" && key!="times_expr")
        throw std::invalid_argument("Unknown script expression property");
    auto it=node.find(key);
    if(it==node.end())throw std::invalid_argument("Expression field does not exist");
    picojson::value* value=&it->second;
    for(const auto& member:path){
        if(member!="left" && member!="right" && member!="operand" &&
           member!="value" && member!="minimum" && member!="maximum")
            throw std::invalid_argument("Unknown expression child path");
        if(!value->is<picojson::object>())
            throw std::invalid_argument("Expression child is not an object");
        auto& children=value->get<picojson::object>();
        auto child=children.find(member);
        if(child==children.end())throw std::invalid_argument("Expression child missing");
        value=&child->second;
    }
    return *value;
}
inline picojson::value defaultExpression(const std::string& op,
                                         const std::string& variable=""){
    if(op=="Variable")return RTSScriptExpression::variable(variable);
    if(op=="Literal")return RTSScriptExpression::number(1);
    const auto unary=[&](){
        return picojson::value(picojson::object{
            {"op",picojson::value(op)},
            {"operand",RTSScriptExpression::number(1)}
        });
    };
    if(op=="Negate" || op=="Absolute")return unary();
    if(RTSScriptExpression::isBinary(op))
        return picojson::value(picojson::object{
            {"op",picojson::value(op)},
            {"left",RTSScriptExpression::number(1)},
            {"right",RTSScriptExpression::number(1)}
        });
    if(op=="Clamp")return picojson::value(picojson::object{
        {"op",picojson::value(op)},{"value",RTSScriptExpression::number(1)},
        {"minimum",RTSScriptExpression::number(0)},
        {"maximum",RTSScriptExpression::number(10)}
    });
    throw std::invalid_argument("Unknown expression operation");
}
inline picojson::object newNode(const RTSProject& project,const std::string& kind,
                                bool condition){
    picojson::object node{{"type",picojson::value(kind)}};
    if(condition){
        if(kind=="PlayerMineralsAtLeast")node["amount"]=picojson::value(100.0);
        else if(kind=="UnitCountAtLeast" || kind=="UnitTypeCountAtLeast"){
            node["count"]=picojson::value(1.0);
            node["owner"]=picojson::value(0.0);
        }else if(kind=="SubjectOwnerIs")node["owner"]=picojson::value(0.0);
        else if(kind=="VariableCompare"){
            node["operator"]=picojson::value(">=");
            node["value"]=picojson::value(1.0);
        }else throw std::invalid_argument("Unknown trigger condition template: "+kind);
    }else{
        if(kind=="ShowMessage")node["text"]=picojson::value("NEW MESSAGE");
        else if(kind=="GrantMinerals")node["amount"]=picojson::value(25.0);
        else if(kind=="SpawnUnit"){
            node["owner"]=picojson::value(0.0);
            node["x"]=picojson::value(double(project.terrain.minX()+
                .5f*(project.terrain.width()-1)*project.terrain.spacing()));
            node["z"]=picojson::value(double(project.terrain.minZ()+
                .5f*(project.terrain.depth()-1)*project.terrain.spacing()));
        }else if(kind=="SetVariable" || kind=="AddVariable")
            node["value"]=picojson::value(1.0);
        else if(kind=="IfThenElse"){
            node["conditions"]=picojson::value(picojson::array{
                picojson::value(newNode(project,"PlayerMineralsAtLeast",true))
            });
            node["then"]=picojson::value(picojson::array{
                picojson::value(newNode(project,"ShowMessage",false))
            });
            node["else"]=picojson::value(picojson::array{});
        }else if(kind=="Repeat"){
            node["times"]=picojson::value(2.0);
            node["actions"]=picojson::value(picojson::array{
                picojson::value(newNode(project,"ShowMessage",false))
            });
        }else if(kind=="EndMatch")node["result"]=picojson::value("Victory");
        else if(kind=="ApplyEffect"){
            for(const auto& entry:project.catalog.entries())
                if(entry.type=="Effect"){node["effect"]=picojson::value(entry.id);break;}
            if(!node.count("effect"))
                throw std::invalid_argument("Create an Effect catalog entry first");
        }else throw std::invalid_argument("Unknown trigger action template: "+kind);
    }
    if(kind=="VariableCompare" || kind=="SetVariable" || kind=="AddVariable"){
        auto it=project.extensions.find("trigger_variables");
        if(it==project.extensions.end() || !it->second.is<picojson::object>() ||
           it->second.get<picojson::object>().empty())
            throw std::invalid_argument("Create a map variable first");
        node["variable"]=picojson::value(
            it->second.get<picojson::object>().begin()->first);
    }
    if(kind=="SpawnUnit" || kind=="UnitTypeCountAtLeast"){
        for(const auto& entry:project.catalog.entries())
            if(entry.type=="Unit"){node["catalog"]=picojson::value(entry.id);break;}
        if(!node.count("catalog"))
            throw std::invalid_argument("Create a Unit catalog entry first");
    }
    return node;
}
} // namespace TriggerTree
