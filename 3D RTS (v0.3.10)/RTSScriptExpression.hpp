#pragma once
#include "third_party/picojson.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

// Bounded deterministic integer expression AST for project-authored scripts.
// All arithmetic is evaluated using signed 64-bit integers then saturated to
// +/-100,000,000. Division/modulo by zero have a defined result of zero.
// No floating point, platform clock, allocation in the evaluator or host calls.
namespace RTSScriptExpression {
constexpr int MaxValue=100000000;
constexpr int MaxDepth=8;
constexpr int MaxNodes=64;
inline int clamped(std::int64_t value) {
    return static_cast<int>(std::clamp(value,std::int64_t(-MaxValue),
                                     std::int64_t(MaxValue)));
}
inline int literal(const picojson::value& value){
    if(!value.is<double>())throw std::invalid_argument("Expression requires integer literal");
    const double n=value.get<double>();
    if(!std::isfinite(n) || std::floor(n)!=n || n<-MaxValue || n>MaxValue)
        throw std::invalid_argument("Expression integer is outside permitted bounds");
    return static_cast<int>(n);
}
inline const picojson::value& field(const picojson::object& o,const char* name){
    const auto it=o.find(name);
    if(it==o.end())throw std::invalid_argument(std::string("Expression missing ")+name);
    return it->second;
}
inline std::string operation(const picojson::object& o){
    const auto& v=field(o,"op");
    if(!v.is<std::string>())throw std::invalid_argument("Expression op must be a string");
    return v.get<std::string>();
}
inline void keys(const picojson::object& o,std::initializer_list<const char*> allowed){
    for(const auto& item:o){
        bool found=false;
        for(const char* key:allowed)if(item.first==key){found=true;break;}
        if(!found)throw std::invalid_argument("Unexpected expression field: "+item.first);
    }
}
inline bool isBinary(const std::string& op){
    return op=="Add" || op=="Subtract" || op=="Multiply" || op=="Divide" ||
           op=="Modulo" || op=="Min" || op=="Max";
}
inline void validateNode(const picojson::value& v,
                         const std::map<std::string,int>& variables,
                         int depth,int& nodes){
    if(depth>=MaxDepth || ++nodes>MaxNodes)
        throw std::invalid_argument("Expression depth/complexity limit exceeded");
    if(!v.is<picojson::object>())
        throw std::invalid_argument("Expression must be an object");
    const auto& o=v.get<picojson::object>();
    const std::string op=operation(o);
    if(op=="Literal"){
        keys(o,{"op","value"});
        (void)literal(field(o,"value"));
    }else if(op=="Variable"){
        keys(o,{"op","name"});
        const auto& n=field(o,"name");
        if(!n.is<std::string>() || variables.count(n.get<std::string>())==0)
            throw std::invalid_argument("Expression references an undeclared variable");
    }else if(op=="Negate" || op=="Absolute"){
        keys(o,{"op","operand"});
        validateNode(field(o,"operand"),variables,depth+1,nodes);
    }else if(isBinary(op)){
        keys(o,{"op","left","right"});
        validateNode(field(o,"left"),variables,depth+1,nodes);
        validateNode(field(o,"right"),variables,depth+1,nodes);
    }else if(op=="Clamp"){
        keys(o,{"op","value","minimum","maximum"});
        validateNode(field(o,"value"),variables,depth+1,nodes);
        validateNode(field(o,"minimum"),variables,depth+1,nodes);
        validateNode(field(o,"maximum"),variables,depth+1,nodes);
    }else throw std::invalid_argument("Unsupported expression operator: "+op);
}
inline void validate(const picojson::value& expression,
                     const std::map<std::string,int>& variables){
    int nodes=0;
    validateNode(expression,variables,0,nodes);
}
inline int evaluate(const picojson::value& expression,
                    const std::map<std::string,int>& variables){
    const auto& o=expression.get<picojson::object>();
    const std::string op=operation(o);
    if(op=="Literal")return literal(field(o,"value"));
    if(op=="Variable")return variables.at(field(o,"name").get<std::string>());
    if(op=="Negate")return clamped(-std::int64_t(evaluate(field(o,"operand"),variables)));
    if(op=="Absolute"){
        const int v=evaluate(field(o,"operand"),variables);
        return v<0?-v:v;
    }
    if(op=="Clamp"){
        const int v=evaluate(field(o,"value"),variables);
        const int lo=evaluate(field(o,"minimum"),variables);
        const int hi=evaluate(field(o,"maximum"),variables);
        return std::clamp(v,std::min(lo,hi),std::max(lo,hi));
    }
    const int a=evaluate(field(o,"left"),variables);
    const int b=evaluate(field(o,"right"),variables);
    if(op=="Add")return clamped(std::int64_t(a)+b);
    if(op=="Subtract")return clamped(std::int64_t(a)-b);
    if(op=="Multiply")return clamped(std::int64_t(a)*b);
    if(op=="Divide")return b==0?0:a/b;
    if(op=="Modulo")return b==0?0:a%b;
    if(op=="Min")return std::min(a,b);
    if(op=="Max")return std::max(a,b);
    throw std::invalid_argument("Unsupported expression operator");
}
inline picojson::value variable(const std::string& name){
    return picojson::value(picojson::object{
        {"op",picojson::value("Variable")},{"name",picojson::value(name)}
    });
}
inline picojson::value number(int n){
    return picojson::value(picojson::object{
        {"op",picojson::value("Literal")},{"value",picojson::value(double(n))}
    });
}
} // namespace RTSScriptExpression
