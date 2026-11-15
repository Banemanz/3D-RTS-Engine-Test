#include "DataCatalog.hpp"
#include "RTSRequirements.hpp"
#include "RTSUpgrades.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <utility>

namespace {
bool validId(const std::string& s) {
    if(s.empty() || s.size()>128)return false;
    for(char c:s)if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') ||
                      (c>='0'&&c<='9') || c=='_' || c=='.' || c=='-')) return false;
    return true;
}
double numeric(const picojson::object& fields,const char* key,double fallback) {
    auto i=fields.find(key);
    if(i==fields.end())return fallback;
    if(!i->second.is<double>())throw std::invalid_argument(std::string(key)+" must be a number");
    double n=i->second.get<double>();
    if(!std::isfinite(n))throw std::invalid_argument(std::string(key)+" must be finite");
    return n;
}
std::string stringField(const picojson::object& fields,const char* key,const std::string& fallback) {
    auto i=fields.find(key);
    if(i==fields.end())return fallback;
    if(!i->second.is<std::string>())throw std::invalid_argument(std::string(key)+" must be a string");
    return i->second.get<std::string>();
}
Vec3 vec3Field(const picojson::object& fields,const char* key,const Vec3& fallback) {
    auto i=fields.find(key);
    if(i==fields.end())return fallback;
    if(!i->second.is<picojson::array>())throw std::invalid_argument(std::string(key)+" must be an array");
    const auto& a=i->second.get<picojson::array>();
    if(a.size()!=3 || !a[0].is<double>() || !a[1].is<double>() || !a[2].is<double>())
        throw std::invalid_argument(std::string(key)+" must contain exactly three numbers");
    Vec3 v(float(a[0].get<double>()),float(a[1].get<double>()),float(a[2].get<double>()));
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))
        throw std::invalid_argument(std::string(key)+" must be finite");
    return v;
}
picojson::value vector3(float x,float y,float z) {
    picojson::array a{picojson::value(double(x)),picojson::value(double(y)),picojson::value(double(z))};
    return picojson::value(a);
}
CatalogEntry make(const std::string& type,const std::string& id,const std::string& parent,
                  picojson::object fields) {
    CatalogEntry e;e.type=type;e.id=id;e.parent=parent;e.fields=std::move(fields);return e;
}
}
const CatalogEntry* DataCatalog::find(const std::string& id)const {
    const auto it=idIndex_.find(id);
    if(it==idIndex_.end() || it->second>=entries_.size())return nullptr;
    const CatalogEntry& entry=entries_[it->second];
    return entry.id==id?&entry:nullptr;
}
CatalogEntry* DataCatalog::find(const std::string& id) {
    const auto it=idIndex_.find(id);
    if(it==idIndex_.end() || it->second>=entries_.size())return nullptr;
    CatalogEntry& entry=entries_[it->second];
    return entry.id==id?&entry:nullptr;
}
bool DataCatalog::add(CatalogEntry e,std::string* error) {
    if(!validId(e.id) || !validId(e.type) || (!e.parent.empty() && !validId(e.parent)) ||
       idIndex_.count(e.id)!=0 || entries_.size()>=10000) {
        if(error)*error="Invalid or duplicate catalog ID/type/parent";
        return false;
    }
    entries_.push_back(std::move(e));
    idIndex_.emplace(entries_.back().id,entries_.size()-1);
    return true;
}
bool DataCatalog::cloneDerived(const std::string& original,const std::string& newId) {
    const CatalogEntry* e=find(original);
    if(!e || !validId(newId) || find(newId))return false;
    return add(make(e->type,newId,original,{}));
}
bool DataCatalog::rename(const std::string& oldId,const std::string& newId) {
    if(oldId==newId || !validId(newId) || !find(oldId) || find(newId))return false;
    validate();
    const std::size_t index=idIndex_.at(oldId);
    const std::string type=entries_[index].type;
    for(auto& entry:entries_) {
        if(entry.parent==oldId)entry.parent=newId;
        const char* ref=(type=="Actor" && entry.type=="Unit")?"actor":
                        (type=="Model" && entry.type=="Actor")?"model":nullptr;
        if(ref) {
            auto field=entry.fields.find(ref);
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Weapon" && entry.type=="Unit"){
            auto field=entry.fields.find("weapon");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Projectile" && (entry.type=="Ability" || entry.type=="Weapon")){
            auto field=entry.fields.find("projectile");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Behavior" && entry.type=="Effect"){
            auto field=entry.fields.find("behavior");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Effect" && entry.type=="Effect"){
            auto field=entry.fields.find("children");
            if(field!=entry.fields.end() && field->second.is<picojson::array>())
                for(auto& child:field->second.get<picojson::array>())
                    if(child.is<std::string>() && child.get<std::string>()==oldId)
                        child=picojson::value(newId);
        }
        if(type=="Ability" && entry.type=="Unit") {
            auto field=entry.fields.find("abilities");
            if(field!=entry.fields.end() && field->second.is<picojson::array>())
                for(auto& candidate:field->second.get<picojson::array>())
                    if(candidate.is<std::string>() &&
                       candidate.get<std::string>()==oldId)
                        candidate=picojson::value(newId);
        }
        if(type=="Effect" && entry.type=="Ability"){
            auto field=entry.fields.find("effect");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Requirement" &&
           (entry.type=="Unit" || entry.type=="Ability" ||
            entry.type=="Upgrade")){
            for(const char* key:{"requirement","show_requirement"}){
                auto field=entry.fields.find(key);
                if(field!=entry.fields.end() && field->second.is<std::string>() &&
                   field->second.get<std::string>()==oldId)
                    field->second=picojson::value(newId);
            }
        }
        if(type=="Requirement" && entry.type=="Requirement"){
            auto field=entry.fields.find("children");
            if(field!=entry.fields.end() && field->second.is<picojson::array>())
                for(auto& child:field->second.get<picojson::array>())
                    if(child.is<std::string>() && child.get<std::string>()==oldId)
                        child=picojson::value(newId);
        }
        if(type=="Validator" && entry.type=="Requirement"){
            auto field=entry.fields.find("validator");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Behavior" && entry.type=="Validator"){
            auto field=entry.fields.find("behavior");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Upgrade" && entry.type=="Validator"){
            auto field=entry.fields.find("upgrade");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Upgrade" && entry.type=="Unit"){
            auto field=entry.fields.find("researchable_upgrades");
            if(field!=entry.fields.end() && field->second.is<picojson::array>())
                for(auto& value:field->second.get<picojson::array>())
                    if(value.is<std::string>() && value.get<std::string>()==oldId)
                        value=picojson::value(newId);
        }
        if(type=="Unit" && entry.type=="Upgrade"){
            auto field=entry.fields.find("unit");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Unit" && entry.type=="Validator"){
            auto field=entry.fields.find("unit");
            if(field!=entry.fields.end() && field->second.is<std::string>() &&
               field->second.get<std::string>()==oldId)
                field->second=picojson::value(newId);
        }
        if(type=="Unit" && entry.type=="Unit") {
            auto field=entry.fields.find("trainable_units");
            if(field!=entry.fields.end() && field->second.is<picojson::array>())
                for(auto& candidate:field->second.get<picojson::array>())
                    if(candidate.is<std::string>() &&
                       candidate.get<std::string>()==oldId)
                        candidate=picojson::value(newId);
        }
    }
    entries_[index].id=newId;
    idIndex_.erase(oldId);
    idIndex_.emplace(newId,index);
    validate();
    return true;
}
bool DataCatalog::erase(const std::string& id) {
    const auto it=idIndex_.find(id);
    if(it==idIndex_.end())return false;
    validate(); // reject any unauthorized direct ID mutation
    const std::size_t at=it->second;
    entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(at));
    idIndex_.erase(id);
    for(std::size_t i=at;i<entries_.size();++i)idIndex_[entries_[i].id]=i;
    return true;
}
bool DataCatalog::setField(const std::string& id,const std::string& key,
                            const picojson::value& value) {
    CatalogEntry* e=find(id);
    if(!e || !validId(key))return false;
    e->fields[key]=value;return true;
}
void DataCatalog::resolveInto(const std::string& id,picojson::object& output,
                              std::vector<std::string>& stack)const {
    const auto* e=find(id);
    if(!e)throw std::invalid_argument("Missing catalog ID: "+id);
    if(std::find(stack.begin(),stack.end(),id)!=stack.end())
        throw std::invalid_argument("Catalog inheritance cycle at: "+id);
    stack.push_back(id);
    if(!e->parent.empty()) {
        const auto* p=find(e->parent);
        if(!p || p->type!=e->type)
            throw std::invalid_argument("Missing/mismatched parent of: "+id);
        resolveInto(e->parent,output,stack);
    }
    for(const auto& field:e->fields)output[field.first]=field.second;
    stack.pop_back();
}
picojson::object DataCatalog::resolvedFields(const std::string& id)const {
    picojson::object result;
    std::vector<std::string> stack;
    resolveInto(id,result,stack);
    return result;
}
BehaviorDefinition compileBehavior(const DataCatalog& catalog,
                                   const std::string& id){
    const auto* entry=catalog.find(id);
    if(!entry || entry->type!="Behavior")
        throw std::invalid_argument("Missing Behavior catalog reference: "+id);
    const auto fields=catalog.resolvedFields(id);
    auto integerField=[&](const char* name,double fallback,int low,int high){
        const double n=numeric(fields,name,fallback);
        if(std::floor(n)!=n || n<double(low) || n>double(high))
            throw std::invalid_argument("Invalid Behavior "+std::string(name)+": "+id);
        return static_cast<int>(n);
    };
    auto finiteField=[&](const char* name,double fallback,double low,double high){
        const double n=numeric(fields,name,fallback);
        if(n<low || n>high)
            throw std::invalid_argument("Invalid Behavior "+std::string(name)+": "+id);
        return static_cast<float>(n);
    };
    auto boolProperty=[&](const char* name,bool fallback){
        const auto it=fields.find(name);
        if(it==fields.end())return fallback;
        if(!it->second.is<bool>())
            throw std::invalid_argument("Behavior "+std::string(name)+" must be boolean: "+id);
        return it->second.get<bool>();
    };
    BehaviorDefinition out;
    out.id=id;
    out.durationTicks=static_cast<std::uint32_t>(
        integerField("duration_ticks",180,1,36000));
    out.maxStacks=integerField("max_stacks",1,1,8);
    out.armorBonus=finiteField("armor_bonus",0.,-50.,50.);
    out.damageBonus=finiteField("damage_bonus",0.,-100.,100.);
    out.speedMultiplier=finiteField("speed_multiplier",1.,.1,3.);
    out.refreshDuration=boolProperty("refresh_duration",true);
    out.dispellable=boolProperty("dispellable",true);
    return out;
}

namespace {
// Bound both graph traversal and terminal effect steps. Cycles and maliciously
// large graphs are rejected before a project can be saved or executed.
void collectEffect(const DataCatalog& catalog,const std::string& id,
                   std::vector<std::string>& stack,
                   std::vector<EffectOperation>& output,std::size_t& visits) {
    const auto* entry=catalog.find(id);
    if(!entry || entry->type!="Effect")
        throw std::invalid_argument("Missing Effect reference: "+id);
    if(stack.size()>=16 || ++visits>64)
        throw std::invalid_argument("Effect graph exceeds depth/step limit: "+id);
    if(std::find(stack.begin(),stack.end(),id)!=stack.end())
        throw std::invalid_argument("Effect graph cycle: "+id);
    stack.push_back(id);
    const auto fields=catalog.resolvedFields(id);
    const auto kind=fields.find("kind");
    if(kind==fields.end() || !kind->second.is<std::string>())
        throw std::invalid_argument("Effect requires kind: "+id);
    const std::string op=kind->second.get<std::string>();
    if(op=="Sequence"){
        auto children=fields.find("children");
        if(children==fields.end() || !children->second.is<picojson::array>())
            throw std::invalid_argument("Sequence requires children: "+id);
        const auto& refs=children->second.get<picojson::array>();
        if(refs.empty() || refs.size()>8)
            throw std::invalid_argument("Sequence requires 1..8 children: "+id);
        for(const auto& ref:refs){
            if(!ref.is<std::string>())
                throw std::invalid_argument("Effect child must be an ID: "+id);
            collectEffect(catalog,ref.get<std::string>(),stack,output,visits);
        }
    }else if(op=="ApplyBehavior" || op=="RemoveBehavior"){
        const auto behavior=fields.find("behavior");
        if(behavior==fields.end() || !behavior->second.is<std::string>())
            throw std::invalid_argument("Effect requires Behavior ID: "+id);
        const std::string target=behavior->second.get<std::string>();
        (void)compileBehavior(catalog,target);
        EffectOperation terminal;
        terminal.kind=op=="ApplyBehavior"?
            EffectOperationKind::ApplyBehavior:EffectOperationKind::RemoveBehavior;
        terminal.behaviorId=target;
        output.push_back(std::move(terminal));
    }else if(op=="Damage" || op=="Heal" ||
             op=="PeriodicDamage" || op=="PeriodicHeal"){
        const auto amount=fields.find("amount");
        if(amount==fields.end() || !amount->second.is<double>() ||
           !std::isfinite(amount->second.get<double>()) ||
           amount->second.get<double>()<=0.0 ||
           amount->second.get<double>()>100000.0)
            throw std::invalid_argument("Effect amount must be 0..100000: "+id);
        EffectOperation terminal;
        terminal.amount=float(amount->second.get<double>());
        terminal.kind=op=="Damage"?EffectOperationKind::Damage:
                      op=="Heal"?EffectOperationKind::Heal:
                      op=="PeriodicDamage"?EffectOperationKind::PeriodicDamage:
                      EffectOperationKind::PeriodicHeal;
        if(op=="PeriodicDamage" || op=="PeriodicHeal"){
            const auto ticks=[&](const char* key,int low,int high){
                const auto it=fields.find(key);
                if(it==fields.end() || !it->second.is<double>())
                    throw std::invalid_argument("Periodic effect requires integer "+std::string(key));
                const double n=it->second.get<double>();
                if(!std::isfinite(n) || std::floor(n)!=n || n<low || n>high)
                    throw std::invalid_argument("Invalid periodic "+std::string(key)+": "+id);
                return static_cast<std::uint32_t>(n);
            };
            terminal.intervalTicks=ticks("interval_ticks",1,600);
            terminal.durationTicks=ticks("duration_ticks",1,3600);
        }
        output.push_back(terminal);
    }else throw std::invalid_argument("Unknown Effect kind: "+op);
    stack.pop_back();
}
} // namespace
std::vector<EffectOperation> compileEffect(const DataCatalog& catalog,
                                           const std::string& id) {
    std::vector<EffectOperation> output;
    std::vector<std::string> stack;
    std::size_t visits=0;
    collectEffect(catalog,id,stack,output,visits);
    return output;
}
ProjectileDefinition compileProjectile(const DataCatalog& catalog,
                                       const std::string& id){
    const auto* entry=catalog.find(id);
    if(!entry || entry->type!="Projectile")
        throw std::invalid_argument("Missing Projectile catalog reference: "+id);
    const auto fields=catalog.resolvedFields(id);
    auto value=[&](const char* field,double fallback,double lo,double hi){
        const double x=numeric(fields,field,fallback);
        if(x<lo || x>hi)
            throw std::invalid_argument("Invalid Projectile "+std::string(field)+": "+id);
        return static_cast<float>(x);
    };
    ProjectileDefinition p;
    p.id=id;
    p.speed=value("speed",12.,.5,100.);
    p.arcHeight=value("arc_height",1.,0.,24.);
    p.visualScale=value("visual_scale",.3,.05,3.);
    p.collisionRadius=value("collision_radius",.35,.05,4.);
    const auto collision=fields.find("collide_units");
    if(collision!=fields.end()){
        if(!collision->second.is<bool>())
            throw std::invalid_argument("Projectile collide_units must be bool: "+id);
        p.collideUnits=collision->second.get<bool>();
    }
    p.tint=vec3Field(fields,"tint",{1.f,.6f,.2f});
    if(p.tint.x<0.f || p.tint.x>1.f ||
       p.tint.y<0.f || p.tint.y>1.f ||
       p.tint.z<0.f || p.tint.z>1.f)
        throw std::invalid_argument("Projectile tint must be RGB [0,1]: "+id);
    return p;
}
WeaponDefinition compileWeapon(const DataCatalog& catalog,
                               const std::string& id){
    const auto* entry=catalog.find(id);
    if(!entry || entry->type!="Weapon")
        throw std::invalid_argument("Missing Weapon catalog reference: "+id);
    const auto fields=catalog.resolvedFields(id);
    WeaponDefinition w;w.id=id;
    auto field=[&](const char* key,double fallback,double lo,double hi){
        const double n=numeric(fields,key,fallback);
        if(n<lo || n>hi)
            throw std::invalid_argument("Invalid Weapon "+std::string(key)+": "+id);
        return static_cast<float>(n);
    };
    w.damage=field("damage",7.,0.,10000.);
    w.range=field("range",6.,0.,128.);
    w.cooldown=field("cooldown",.75,.05,60.);
    w.armorPiercing=field("armor_piercing",0.,0.,1000.);
    w.projectileId=stringField(fields,"projectile","");
    if(!w.projectileId.empty())(void)compileProjectile(catalog,w.projectileId);
    return w;
}
AbilityDefinition compileAbility(const DataCatalog& catalog,
                                 const std::string& id){
    const auto* entry=catalog.find(id);
    if(!entry || entry->type!="Ability")
        throw std::invalid_argument("Missing Ability catalog reference: "+id);
    const auto fields=catalog.resolvedFields(id);
    AbilityDefinition ability;
    ability.id=id;
    ability.requirementId=stringField(fields,"requirement","");
    if(!ability.requirementId.empty())
        (void)compileRequirement(catalog,ability.requirementId);
    ability.showRequirementId=stringField(fields,"show_requirement","");
    if(!ability.showRequirementId.empty()){
        const auto graph=compileRequirement(catalog,ability.showRequirementId);
        if(rtsRequirementUsesTarget(graph))
            throw std::invalid_argument(
                "Ability Show Requirement cannot inspect Target: "+id);
    }
    ability.effect=stringField(fields,"effect","");
    if(ability.effect.empty())
        throw std::invalid_argument("Ability requires an Effect: "+id);
    const auto* effect=catalog.find(ability.effect);
    if(!effect || effect->type!="Effect")
        throw std::invalid_argument("Ability references invalid Effect: "+id);
    (void)compileEffect(catalog,ability.effect);

    const std::string target=stringField(fields,"target","Self");
    if(target=="Self")ability.target=AbilityTarget::Self;
    else if(target=="Ally")ability.target=AbilityTarget::Ally;
    else if(target=="Enemy")ability.target=AbilityTarget::Enemy;
    else if(target=="Any")ability.target=AbilityTarget::Any;
    else if(target=="Ground")ability.target=AbilityTarget::Ground;
    else throw std::invalid_argument("Invalid Ability target type: "+id);
    const double cooldown=numeric(fields,"cooldown_ticks",60.);
    const double range=numeric(fields,"cast_range",ability.target==AbilityTarget::Self?0.:8.);
    const double minerals=numeric(fields,"mineral_cost",0.);
    const double energy=numeric(fields,"energy_cost",0.);
    if(std::floor(cooldown)!=cooldown || cooldown<0 || cooldown>36000 ||
       range<0 || range>128 ||
       std::floor(minerals)!=minerals || minerals<0 || minerals>100000 ||
       std::floor(energy)!=energy || energy<0 || energy>10000)
        throw std::invalid_argument("Invalid Ability cost/range/cooldown: "+id);
    ability.cooldownTicks=static_cast<std::uint32_t>(cooldown);
    ability.castRange=static_cast<float>(range);
    ability.mineralCost=static_cast<int>(minerals);
    ability.energyCost=static_cast<int>(energy);
    if(ability.target!=AbilityTarget::Ground &&
       !stringField(fields,"projectile","").empty())
        throw std::invalid_argument("Projectile reference requires Ground Ability: "+id);
    if(ability.target==AbilityTarget::Ground){
        const double radius=numeric(fields,"area_radius",3.);
        const double delay=numeric(fields,"impact_delay_ticks",0.);
        const std::string team=stringField(fields,"area_team","Enemy");
        const std::string shape=stringField(fields,"area_shape","Circle");
        const double angle=numeric(fields,"area_angle",90.);
        const double width=numeric(fields,"area_width",2.);
        if(!std::isfinite(angle) || angle<10. || angle>170. ||
           !std::isfinite(width) || width<.25 || width>12.)
            throw std::invalid_argument("Invalid Ground area arc/line width: "+id);
        if(shape=="Circle")ability.areaShape=RTSAreaShape::Circle;
        else if(shape=="Cone")ability.areaShape=RTSAreaShape::Cone;
        else if(shape=="Line")ability.areaShape=RTSAreaShape::Line;
        else throw std::invalid_argument("Invalid Ground area shape: "+id);
        ability.areaAngle=static_cast<float>(angle);
        ability.areaWidth=static_cast<float>(width);
        if(radius<.5 || radius>24. ||
           std::floor(delay)!=delay || delay<0 || delay>300)
            throw std::invalid_argument("Invalid Ground area radius/impact delay: "+id);
        ability.areaRadius=static_cast<float>(radius);
        ability.projectileId=stringField(fields,"projectile","");
        if(!ability.projectileId.empty())
            (void)compileProjectile(catalog,ability.projectileId);
        ability.impactDelayTicks=static_cast<std::uint32_t>(delay);
        if(team=="Enemy")ability.areaTeam=AbilityAreaTeam::Enemy;
        else if(team=="Ally")ability.areaTeam=AbilityAreaTeam::Ally;
        else if(team=="Any")ability.areaTeam=AbilityAreaTeam::Any;
        else throw std::invalid_argument("Invalid Ground area team filter: "+id);
        if(ability.castRange<.5f)
            throw std::invalid_argument("Ground Ability requires a nonzero cast_range: "+id);
        if(!ability.requirementId.empty() &&
           rtsRequirementUsesTarget(
                compileRequirement(catalog,ability.requirementId)))
            throw std::invalid_argument(
                "Ground Ability Requirement cannot inspect Target: "+id);
    }
    return ability;
}
std::vector<std::string> unitAbilityIds(const DataCatalog& catalog,
                                         const std::string& unitId){
    const auto* entry=catalog.find(unitId);
    if(!entry || entry->type!="Unit")
        throw std::invalid_argument("Missing Unit for abilities: "+unitId);
    const auto fields=catalog.resolvedFields(unitId);
    const auto refs=fields.find("abilities");
    if(refs==fields.end())return {};
    if(!refs->second.is<picojson::array>())
        throw std::invalid_argument("Unit abilities must be an array: "+unitId);
    const auto& entries=refs->second.get<picojson::array>();
    if(entries.size()>8)
        throw std::invalid_argument("Unit exceeds 8 Abilities: "+unitId);
    std::vector<std::string> ids;
    std::set<std::string> unique;
    for(const auto& value:entries){
        if(!value.is<std::string>())
            throw std::invalid_argument("Unit abilities must contain string IDs: "+unitId);
        const std::string id=value.get<std::string>();
        if(!unique.insert(id).second)
            throw std::invalid_argument("Duplicate Ability on Unit: "+unitId);
        (void)compileAbility(catalog,id);
        ids.push_back(id);
    }
    return ids;
}
void DataCatalog::validate()const {
    if(idIndex_.size()!=entries_.size())
        throw std::invalid_argument("Catalog ID index is inconsistent");
    for(std::size_t i=0;i<entries_.size();++i) {
        const auto it=idIndex_.find(entries_[i].id);
        if(it==idIndex_.end() || it->second!=i)
            throw std::invalid_argument("Catalog entry IDs were changed outside the catalog API");
    }
    std::set<std::string> ids;
    for(const auto& e:entries_) {
        if(!validId(e.id) || !validId(e.type) || (!e.parent.empty() && !validId(e.parent)))
            throw std::invalid_argument("Invalid catalog type or ID");
        if(!ids.insert(e.id).second)throw std::invalid_argument("Duplicate catalog ID: "+e.id);
        (void)resolvedFields(e.id);
    }
    // Only known engine-facing references are checked; arbitrary extension
    // fields are allowed and round-trip losslessly.
    for(const auto& e:entries_) {
        auto fields=resolvedFields(e.id);
        const char* reference = e.type=="Unit"?"actor":(e.type=="Actor"?"model":nullptr);
        const char* type = e.type=="Unit"?"Actor":"Model";
        if(e.type=="Upgrade")(void)compileUpgrade(*this,e.id);
        if(e.type=="Validator")(void)compileValidator(*this,e.id);
        if(e.type=="Requirement")(void)compileRequirement(*this,e.id);
        if(e.type=="Actor")(void)compileActorCues(*this,e.id);
        if(e.type=="Weapon")(void)compileWeapon(*this,e.id);
        if(e.type=="Projectile")(void)compileProjectile(*this,e.id);
        if(e.type=="Behavior")(void)compileBehavior(*this,e.id);
        if(e.type=="Effect")(void)compileEffect(*this,e.id);
        if(e.type=="Ability")(void)compileAbility(*this,e.id);
        if(e.type=="Unit"){
            (void)unitAbilityIds(*this,e.id);
            (void)unitResearchIds(*this,e.id);
            // Check the optional typed reference even on unused Unit catalog
            // entries. Avoid recompiling every Unit's unrelated body/actor
            // data during each F7 edit or changing legacy validation policy.
            const auto weapon=stringField(fields,"weapon","");
            if(!weapon.empty())(void)compileWeapon(*this,weapon);
            const auto show=stringField(fields,"show_requirement","");
            if(!show.empty()){
                const auto graph=compileRequirement(*this,show);
                if(rtsRequirementUsesTarget(graph))
                    throw std::invalid_argument(
                        "Unit Show Requirement cannot inspect Target: "+e.id);
            }
            const auto required=stringField(fields,"requirement","");
            if(!required.empty()){
                const auto compiled=compileRequirement(*this,required);
                if(rtsRequirementUsesTarget(compiled))
                    throw std::invalid_argument(
                        "Unit training Requirement cannot inspect Target: "+e.id);
            }
        }
        if(reference && fields.count(reference)) {
            std::string id=stringField(fields,reference,"");
            const auto* linked=find(id);
            if(!id.empty() && (!linked || linked->type!=type))
                throw std::invalid_argument("Broken "+std::string(reference)+" reference in "+e.id);
        }
    }
}
picojson::value DataCatalog::serialize()const {
    picojson::array result;
    for(const auto& e:entries_) {
        picojson::object ob;
        ob["id"]=picojson::value(e.id);
        ob["type"]=picojson::value(e.type);
        if(!e.parent.empty())ob["parent"]=picojson::value(e.parent);
        ob["fields"]=picojson::value(e.fields);
        result.emplace_back(ob);
    }
    return picojson::value(result);
}
DataCatalog DataCatalog::deserialize(const picojson::value& value) {
    if(!value.is<picojson::array>())throw std::invalid_argument("catalog must be an array");
    DataCatalog result;
    for(const auto& v:value.get<picojson::array>()) {
        if(!v.is<picojson::object>())throw std::invalid_argument("catalog entry must be object");
        const auto& ob=v.get<picojson::object>();
        if(!ob.count("id")||!ob.count("type")||!ob.count("fields")||
           !ob.at("id").is<std::string>()||!ob.at("type").is<std::string>()||
           !ob.at("fields").is<picojson::object>())
            throw std::invalid_argument("catalog entry requires id,type,fields");
        CatalogEntry e;
        e.id=ob.at("id").get<std::string>();e.type=ob.at("type").get<std::string>();
        e.fields=ob.at("fields").get<picojson::object>();
        if(ob.count("parent")) {
            if(!ob.at("parent").is<std::string>())throw std::invalid_argument("parent must be string");
            e.parent=ob.at("parent").get<std::string>();
        }
        if(!result.add(std::move(e)))throw std::invalid_argument("Invalid/duplicate catalog entry");
    }
    result.validate();
    return result;
}
DataCatalog DataCatalog::defaults() {
    DataCatalog c;
    c.add(make("Model","procedural_soldier","",{
        {"mesh",picojson::value("procedural.unit")},
        {"scale",vector3(1,1,1)},{"material",picojson::value("default")}
    }));
    c.add(make("Model","procedural_building","",{
        {"mesh",picojson::value("procedural.unit")},{"scale",vector3(1,1,1)}
    }));
    c.add(make("Actor","ranger_actor","",{
        {"model",picojson::value("procedural_soldier")},
        {"tint",vector3(.22f,.58f,.96f)},{"opacity",picojson::value(1.0)}
    }));
    c.add(make("Actor","worker_actor","ranger_actor",{
        {"tint",vector3(.20f,.90f,.65f)}
    }));
    c.add(make("Actor","rift_raider_actor","",{
        {"model",picojson::value("procedural_soldier")},
        {"tint",vector3(.76f,.24f,.88f)}
    }));
    c.add(make("Actor","human_building_actor","",{
        {"model",picojson::value("procedural_building")},
        {"tint",vector3(.24f,.61f,.86f)}
    }));
    c.add(make("Actor","hybrid_building_actor","human_building_actor",{
        {"tint",vector3(.72f,.24f,.85f)}
    }));
    c.add(make("Unit","ranger","",{
        {"name",picojson::value("Frontier Ranger")},{"actor",picojson::value("ranger_actor")},
        {"faction",picojson::value("Humanity")},{"max_hp",picojson::value(45.0)},
        {"armor",picojson::value(0.0)},{"damage",picojson::value(7.0)},
        {"attack_range",picojson::value(6.5)},{"attack_cooldown",picojson::value(.75)},
        {"aggro_range",picojson::value(10.5)},{"sight_range",picojson::value(14.0)},
        {"move_speed",picojson::value(9.5)},
        {"supply_cost",picojson::value(1.0)},{"mineral_cost",picojson::value(50.0)},
        {"build_time",picojson::value(6.0)},{"visual_scale",picojson::value(.92)}
    }));
    c.add(make("Unit","worker","ranger",{
        {"name",picojson::value("Human Worker")},{"actor",picojson::value("worker_actor")},
        {"max_hp",picojson::value(40.0)},{"damage",picojson::value(2.0)},
        {"attack_range",picojson::value(1.5)},{"attack_cooldown",picojson::value(1.3)},
        {"move_speed",picojson::value(7.0)},{"visual_scale",picojson::value(.72)},
        {"mineral_cost",picojson::value(40.0)},{"sight_range",picojson::value(12.0)},
        {"is_worker",picojson::value(true)},{"build_time",picojson::value(4.5)}
    }));
    c.add(make("Unit","rift_raider","",{
        {"name",picojson::value("Rift Raider")},{"actor",picojson::value("rift_raider_actor")},
        {"faction",picojson::value("Hybrid")},{"max_hp",picojson::value(160.0)},
        {"armor",picojson::value(3.0)},{"damage",picojson::value(18.0)},
        {"attack_range",picojson::value(1.85)},{"attack_cooldown",picojson::value(1.05)},
        {"aggro_range",picojson::value(11.0)},{"sight_range",picojson::value(14.0)},
        {"move_speed",picojson::value(8.2)},
        {"supply_cost",picojson::value(3.0)},{"mineral_cost",picojson::value(150.0)},
        {"visual_scale",picojson::value(1.28)}
    }));
    c.add(make("Unit","command_center","ranger",{
        {"name",picojson::value("Command Center")},{"actor",picojson::value("human_building_actor")},
        {"max_hp",picojson::value(700.0)},{"armor",picojson::value(2.0)},
        {"damage",picojson::value(0.0)},{"attack_range",picojson::value(0.0)},
        {"aggro_range",picojson::value(0.0)},{"sight_range",picojson::value(18.0)},
        {"visual_scale",picojson::value(2.8)},
        {"supply_cost",picojson::value(0.0)},{"is_structure",picojson::value(true)},
        {"trainable_units",picojson::value(picojson::array{picojson::value("worker")})}
    }));
    c.add(make("Unit","rift_bastion","command_center",{
        {"name",picojson::value("Rift Bastion")},{"actor",picojson::value("hybrid_building_actor")},
        {"faction",picojson::value("Hybrid")}
    }));
    c.add(make("Unit","barracks","command_center",{
        {"name",picojson::value("Barracks")},{"max_hp",picojson::value(350.0)},
        {"visual_scale",picojson::value(2.0)},{"build_time",picojson::value(9.0)},
        {"mineral_cost",picojson::value(150.0)},
        {"trainable_units",picojson::value(picojson::array{picojson::value("ranger")})}
    }));
    // Original, tool-visible effect templates; consumed by the trigger
    // runtime and future unit abilities. Neither is tied to a specific IP.
    c.add(make("Effect","restorative_pulse","",{
        {"kind",picojson::value("Heal")},{"amount",picojson::value(8.0)}
    }));
    c.add(make("Effect","kinetic_pulse","",{
        {"kind",picojson::value("Damage")},{"amount",picojson::value(12.0)}
    }));
    c.add(make("Effect","stinging_cloud","",{
        {"kind",picojson::value("PeriodicDamage")},
        {"amount",picojson::value(3.0)},
        {"interval_ticks",picojson::value(60.0)},
        {"duration_ticks",picojson::value(300.0)}
    }));
    c.add(make("Effect","renewal_field","",{
        {"kind",picojson::value("PeriodicHeal")},
        {"amount",picojson::value(4.0)},
        {"interval_ticks",picojson::value(60.0)},
        {"duration_ticks",picojson::value(240.0)}
    }));
    c.validate();
    return c;
}
UnitData unitFromCatalog(const DataCatalog& catalog,const std::string& id) {
    const auto* item=catalog.find(id);
    if(!item || item->type!="Unit")throw std::invalid_argument("Not a Unit catalog entry: "+id);
    auto f=catalog.resolvedFields(id);
    UnitData d;
    d.resourceName=stringField(f,"name",id);
    const auto faction=stringField(f,"faction","Humanity");
    if(faction=="Humanity")d.faction=Faction::Humanity;
    else if(faction=="Hybrid")d.faction=Faction::Hybrid;
    else throw std::invalid_argument("Unsupported faction: "+faction);
    d.max_hp=float(numeric(f,"max_hp",d.max_hp));
    d.hp_regen=float(numeric(f,"hp_regen",d.hp_regen));
    d.max_energy=float(numeric(f,"max_energy",d.max_energy));
    d.energy_regen=float(numeric(f,"energy_regen",d.energy_regen));
    d.armor=float(numeric(f,"armor",d.armor));
    d.damage=float(numeric(f,"damage",d.damage));
    d.attack_range=float(numeric(f,"attack_range",d.attack_range));
    d.attack_cooldown=float(numeric(f,"attack_cooldown",d.attack_cooldown));
    d.weaponId=stringField(f,"weapon","");
    if(!d.weaponId.empty()){
        const auto weapon=compileWeapon(catalog,d.weaponId);
        d.damage=weapon.damage;
        d.attack_range=weapon.range;
        d.attack_cooldown=weapon.cooldown;
        d.armor_piercing=weapon.armorPiercing;
        d.projectileId=weapon.projectileId;
    }
    d.aggro_range=float(numeric(f,"aggro_range",d.aggro_range));
    d.sight_range=float(numeric(f,"sight_range",d.sight_range));
    d.move_speed=float(numeric(f,"move_speed",d.move_speed));
    d.supply_cost=int(numeric(f,"supply_cost",d.supply_cost));
    d.mineral_cost=int(numeric(f,"mineral_cost",d.mineral_cost));
    d.build_time=float(numeric(f,"build_time",d.build_time));
    d.visual_scale=float(numeric(f,"visual_scale",d.visual_scale));
    if(d.max_hp<=0 || d.hp_regen<0 || d.hp_regen>1000 ||
       d.max_energy<0 || d.max_energy>10000 ||
       d.energy_regen<0 || d.energy_regen>1000 ||
       d.armor<0 || d.damage<0 || d.attack_range<0 ||
       d.attack_cooldown<=0 || d.aggro_range<0 ||
       d.sight_range<0 || d.sight_range>128 || d.move_speed<0 ||
       d.supply_cost<0 || d.mineral_cost<0 || d.build_time<0 || d.visual_scale<=0)
        throw std::invalid_argument("Invalid unit gameplay stats: "+id);
    std::string actor=stringField(f,"actor","");
    if(!actor.empty()) {
        ActorVisual visual=actorFromCatalog(catalog,actor);
        d.faction_color=visual.tint;
    }
    return d;
}
std::vector<RTSActorCueDefinition> compileActorCues(
    const DataCatalog& catalog,const std::string& actorId){
    const auto* actor=catalog.find(actorId);
    if(!actor || actor->type!="Actor")
        throw std::invalid_argument("Missing Actor catalog reference: "+actorId);
    const auto fields=catalog.resolvedFields(actorId);
    const auto it=fields.find("events");
    if(it==fields.end())return {};
    if(!it->second.is<picojson::array>())
        throw std::invalid_argument("Actor events must be an array: "+actorId);
    const auto& array=it->second.get<picojson::array>();
    if(array.size()>12)
        throw std::invalid_argument("Actor events exceed twelve cues: "+actorId);
    std::vector<RTSActorCueDefinition> result;
    result.reserve(array.size());
    for(const auto& item:array){
        if(!item.is<picojson::object>())
            throw std::invalid_argument("Actor event entry must be an object: "+actorId);
        const auto& properties=item.get<picojson::object>();
        RTSActorCueDefinition cue;
        RTSActorCueEvent event;
        RTSActorCueStyle style;
        if(!rtsParseActorCueEvent(stringField(properties,"when",""),event) ||
           !rtsParseActorCueStyle(stringField(properties,"style",""),style))
            throw std::invalid_argument("Unknown Actor event/style: "+actorId);
        cue.event=event;cue.style=style;
        cue.tint=vec3Field(properties,"tint",cue.tint);
        if(cue.tint.x<0.f || cue.tint.x>1.f || cue.tint.y<0.f ||
           cue.tint.y>1.f || cue.tint.z<0.f || cue.tint.z>1.f)
            throw std::invalid_argument("Actor event tint must be RGB [0,1]: "+actorId);
        const double radius=numeric(properties,"radius",cue.radius);
        const double ticks=numeric(properties,"duration_ticks",cue.durationTicks);
        if(radius<.25 || radius>12. || ticks<1. || ticks>180. ||
           std::floor(ticks)!=ticks)
            throw std::invalid_argument("Invalid Actor cue size/duration: "+actorId);
        cue.radius=static_cast<float>(radius);
        cue.durationTicks=static_cast<std::uint32_t>(ticks);
        result.push_back(cue);
    }
    return result;
}
ActorVisual actorFromCatalog(const DataCatalog& catalog,const std::string& actorId) {
    const auto* entry=catalog.find(actorId);
    if(!entry || entry->type!="Actor")throw std::invalid_argument("Not an Actor: "+actorId);
    auto f=catalog.resolvedFields(actorId);
    ActorVisual v;
    v.tint=vec3Field(f,"tint",v.tint);
    v.scale=vec3Field(f,"scale",v.scale);
    v.opacity=float(numeric(f,"opacity",1.0));
    v.animation=stringField(f,"animation","");
    v.sound=stringField(f,"sound","");
    v.cues=compileActorCues(catalog,actorId);
    auto modelId=stringField(f,"model","");
    if(!modelId.empty()){
        auto m=catalog.resolvedFields(modelId);
        v.mesh=stringField(m,"mesh",v.mesh);
        v.texture=stringField(m,"texture","");
        Vec3 modelScale=vec3Field(m,"scale",{1,1,1});
        v.scale=Vec3(v.scale.x*modelScale.x,v.scale.y*modelScale.y,v.scale.z*modelScale.z);
    }
    if(v.opacity<0 || v.opacity>1 || v.scale.x<=0 || v.scale.y<=0 || v.scale.z<=0)
        throw std::invalid_argument("Invalid visual scale or opacity: "+actorId);
    return v;
}
