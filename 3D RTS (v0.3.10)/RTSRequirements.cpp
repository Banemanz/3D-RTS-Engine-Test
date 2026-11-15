#include "RTSRequirements.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
double numeric(const picojson::object& f,const char* key,double fallback){
    const auto it=f.find(key);
    if(it==f.end())return fallback;
    if(!it->second.is<double>())
        throw std::invalid_argument(std::string(key)+" must be a number");
    const double v=it->second.get<double>();
    if(!std::isfinite(v))
        throw std::invalid_argument(std::string(key)+" must be finite");
    return v;
}
std::string str(const picojson::object& f,const char* key,
                const std::string& fallback=""){
    const auto it=f.find(key);
    if(it==f.end())return fallback;
    if(!it->second.is<std::string>())
        throw std::invalid_argument(std::string(key)+" must be a string");
    return it->second.get<std::string>();
}
RTSRequirementDefinition recurse(const DataCatalog& c,const std::string& id,
                                  std::vector<std::string>& stack,
                                  std::size_t& visits){
    if(stack.size()>=8 || visits>=64)
        throw std::invalid_argument("Requirement graph exceeds depth/nodes: "+id);
    if(std::find(stack.begin(),stack.end(),id)!=stack.end())
        throw std::invalid_argument("Cyclic Requirement reference: "+id);
    const auto* item=c.find(id);
    if(!item || item->type!="Requirement")
        throw std::invalid_argument("Expected Requirement reference: "+id);
    ++visits;
    stack.push_back(id);
    RTSRequirementDefinition result;
    result.id=id;
    const auto fields=c.resolvedFields(id);
    const auto type=str(fields,"kind","Predicate");
    if(type=="Predicate"){
        result.kind=RTSRequirementKind::Predicate;
        const auto ref=str(fields,"validator");
        if(ref.empty())throw std::invalid_argument("Requirement missing Validator: "+id);
        result.predicate=compileValidator(c,ref);
    }else if(type=="All" || type=="Any" || type=="Not"){
        result.kind=type=="All"?RTSRequirementKind::All:
                    type=="Any"?RTSRequirementKind::Any:RTSRequirementKind::Not;
        const auto it=fields.find("children");
        if(it==fields.end() || !it->second.is<picojson::array>())
            throw std::invalid_argument("Requirement needs child references: "+id);
        const auto& refs=it->second.get<picojson::array>();
        if(refs.empty() || refs.size()>8 ||
           (result.kind==RTSRequirementKind::Not && refs.size()!=1))
            throw std::invalid_argument("Requirement child count invalid: "+id);
        std::set<std::string> unique;
        for(const auto& value:refs){
            if(!value.is<std::string>())
                throw std::invalid_argument("Requirement child must be ID: "+id);
            const auto& child=value.get<std::string>();
            if(!unique.insert(child).second)
                throw std::invalid_argument("Duplicate Requirement child: "+child);
            result.children.push_back(recurse(c,child,stack,visits));
        }
    }else throw std::invalid_argument("Unknown Requirement kind: "+type);
    stack.pop_back();
    return result;
}
}
const char* rtsValidatorKindName(RTSValidatorKind kind){
    switch(kind){
        case RTSValidatorKind::HealthBelowPercent:return "HealthBelowPercent";
        case RTSValidatorKind::EnergyAtLeast:return "EnergyAtLeast";
        case RTSValidatorKind::HasBehavior:return "HasBehavior";
        case RTSValidatorKind::IsStructure:return "IsStructure";
        case RTSValidatorKind::OwnerUnitCountAtLeast:return "OwnerUnitCountAtLeast";
    }
    return "";
}
const char* rtsValidatorSubjectName(RTSValidatorSubject subject){
    return subject==RTSValidatorSubject::Caster?"Caster":"Target";
}
const char* rtsRequirementKindName(RTSRequirementKind kind){
    switch(kind){
        case RTSRequirementKind::Predicate:return "Predicate";
        case RTSRequirementKind::All:return "All";
        case RTSRequirementKind::Any:return "Any";
        case RTSRequirementKind::Not:return "Not";
    }
    return "";
}
RTSValidatorDefinition compileValidator(const DataCatalog& catalog,
                                        const std::string& id){
    const auto* item=catalog.find(id);
    if(!item || item->type!="Validator")
        throw std::invalid_argument("Expected Validator reference: "+id);
    const auto fields=catalog.resolvedFields(id);
    RTSValidatorDefinition out;out.id=id;
    const std::string kind=str(fields,"kind","HealthBelowPercent");
    const std::string subject=str(fields,"subject","Caster");
    bool matched=false;
    for(int i=0;i<5;++i){
        const auto value=static_cast<RTSValidatorKind>(i);
        if(kind==rtsValidatorKindName(value)){out.kind=value;matched=true;break;}
    }
    if(!matched)throw std::invalid_argument("Unsupported Validator kind: "+kind);
    if(subject=="Caster")out.subject=RTSValidatorSubject::Caster;
    else if(subject=="Target")out.subject=RTSValidatorSubject::Target;
    else throw std::invalid_argument("Unsupported Validator subject: "+subject);
    if(out.kind==RTSValidatorKind::HealthBelowPercent ||
       out.kind==RTSValidatorKind::EnergyAtLeast ||
       out.kind==RTSValidatorKind::OwnerUnitCountAtLeast){
        const double amount=numeric(fields,"value",
            out.kind==RTSValidatorKind::HealthBelowPercent?50.:
            out.kind==RTSValidatorKind::EnergyAtLeast?25.:1.);
        const double minimum=out.kind==RTSValidatorKind::OwnerUnitCountAtLeast?1.:0.;
        const double maximum=out.kind==RTSValidatorKind::HealthBelowPercent?100.:
                             out.kind==RTSValidatorKind::EnergyAtLeast?10000.:4000.;
        if(amount<minimum || amount>maximum ||
           (out.kind==RTSValidatorKind::OwnerUnitCountAtLeast &&
            std::floor(amount)!=amount))
            throw std::invalid_argument("Invalid Validator value: "+id);
        out.value=static_cast<float>(amount);
    }
    if(out.kind==RTSValidatorKind::HasBehavior){
        out.behaviorId=str(fields,"behavior");
        if(out.behaviorId.empty() || !catalog.find(out.behaviorId) ||
           catalog.find(out.behaviorId)->type!="Behavior")
            throw std::invalid_argument("Validator needs a Behavior: "+id);
        (void)compileBehavior(catalog,out.behaviorId);
    }
    if(out.kind==RTSValidatorKind::OwnerUnitCountAtLeast){
        out.unitId=str(fields,"unit");
        if(!out.unitId.empty() &&
           (!catalog.find(out.unitId) || catalog.find(out.unitId)->type!="Unit"))
            throw std::invalid_argument("Validator has invalid Unit reference: "+id);
    }
    return out;
}
RTSRequirementDefinition compileRequirement(const DataCatalog& catalog,
                                            const std::string& id){
    std::vector<std::string> stack;
    std::size_t visits=0;
    return recurse(catalog,id,stack,visits);
}
