#include "EditorSession.hpp"
#include "TriggerDefinition.hpp"
#include "TriggerTree.hpp"
#include "RTSScriptExpression.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <cstddef>
#include <cctype>

std::string EditorSession::panelName()const {
    switch(panel) {
        case Panel::Play:return "PLAY";
        case Panel::Terrain:return "TERRAIN EDITOR";
        case Panel::Data:return "DATA EDITOR";
        case Panel::Triggers:return "TRIGGER EDITOR";
        case Panel::Scene:return "SCENE EDITOR";
    }
    return "";
}
std::string EditorSession::toolName()const {
    switch(tool) {
        case Tool::Raise:return "RAISE";
        case Tool::Lower:return "LOWER";
        case Tool::Smooth:return "SMOOTH";
        case Tool::Flatten:return "FLATTEN";
        case Tool::Paint:return "TEXTURE";
        case Tool::Block:return "PATH BLOCK";
        case Tool::Unblock:return "PATH ERASE";
        case Tool::PlaceMineral:return "PLACE MINERAL";
        case Tool::PlaceUnit:return "PLACE UNIT";
        case Tool::PlaceDoodad:return "PLACE DOODAD";
    }
    return "";
}
void EditorSession::startStroke(RTSProject& project) {
    if(painting_)return;
    painting_=true;strokeActionRecorded_=false;
    strokeBefore_=std::make_unique<RTSProject>(project);
    strokeBefore_->terrain.clearHistory();
    // One full-project snapshot owns this edit; don't duplicate terrain history.
    project.terrain.beginStroke(false);
}
void EditorSession::finishStroke(RTSProject& project) {
    if(!painting_)return;
    painting_=false;
    strokeActionRecorded_=false;
    project.terrain.endStroke();
    strokeBefore_.reset();
}
bool EditorSession::paint(RTSProject& p,const Vec3& worldPos) {
    if(panel!=Panel::Terrain || !painting_)return false;
    if(tool==Tool::PlaceMineral || tool==Tool::PlaceUnit || tool==Tool::PlaceDoodad)
        return false;
    TerrainMap::Brush kind;
    switch(tool) {
        case Tool::Raise:kind=TerrainMap::Brush::Raise;break;
        case Tool::Lower:kind=TerrainMap::Brush::Lower;break;
        case Tool::Smooth:kind=TerrainMap::Brush::Smooth;break;
        case Tool::Flatten:kind=TerrainMap::Brush::Flatten;break;
        case Tool::Paint:kind=TerrainMap::Brush::Paint;break;
        case Tool::Block:kind=TerrainMap::Brush::Block;break;
        case Tool::Unblock:kind=TerrainMap::Brush::Unblock;break;
        default:return false;
    }
    bool changed=p.terrain.applyBrush(worldPos,kind,brushRadius,brushStrength,
                                      static_cast<std::uint8_t>(materialId));
    if(changed){
        modified=true;
        if(!strokeActionRecorded_) {
            strokeActionRecorded_=true;
            if(strokeBefore_)remember(*strokeBefore_);
            strokeBefore_.reset();
        }
    }
    return changed;
}
void EditorSession::remember(const RTSProject& project) {
    undo_.push_back(project);
    undo_.back().terrain.clearHistory();
    while(undo_.size()>maxHistory_)undo_.pop_front();
    redo_.clear();
}
bool EditorSession::undo(RTSProject& project) {
    finishStroke(project);
    if(undo_.empty())return false;
    redo_.push_back(project);
    redo_.back().terrain.clearHistory();
    while(redo_.size()>maxHistory_)redo_.pop_front();
    project=std::move(undo_.back());
    undo_.pop_back();
    project.terrain.markDirty();
    modified=true;
    return true;
}
bool EditorSession::redo(RTSProject& project) {
    finishStroke(project);
    if(redo_.empty())return false;
    undo_.push_back(project);
    undo_.back().terrain.clearHistory();
    while(undo_.size()>maxHistory_)undo_.pop_front();
    project=std::move(redo_.back());
    redo_.pop_back();
    project.terrain.markDirty();
    modified=true;
    return true;
}
void EditorSession::discardRejectedChange(){
    if(!undo_.empty())undo_.pop_back();
}
bool EditorSession::cyclePlacementCatalog(const RTSProject& project,int direction,bool doodad) {
    std::vector<std::string> ids;
    for(const auto& e:project.catalog.entries())
        if(doodad ? e.type=="Actor" : e.type=="Unit")ids.push_back(e.id);
    if(ids.empty())return false;
    std::string& current=doodad?placementDoodad:placementArchetype;
    auto i=std::find(ids.begin(),ids.end(),current);
    int idx=(i==ids.end()?0:int(i-ids.begin()));
    idx=(idx+direction+int(ids.size()))%int(ids.size());
    current=ids[static_cast<std::size_t>(idx)];
    return true;
}
bool EditorSession::place(RTSProject& project,const Vec3& world) {
    if(panel!=Panel::Terrain || !project.terrain.insideWorld(world.x,world.z))
        return false;
    ProjectPlacement item;
    item.id="placed_"+std::to_string(project.placements.size()+1);
    std::set<std::string> existing;
    for(const auto& p:project.placements)existing.insert(p.id);
    int n=1;
    while(existing.count(item.id))item.id="placed_"+std::to_string(project.placements.size()+1+n++);
    item.position=project.terrain.clampToMap(world);
    item.owner=placementOwner;
    if(tool==Tool::PlaceMineral) {
        item.kind="Mineral";item.name="Mineral Deposit";item.owner=-1;item.amount=750;
    }else if(tool==Tool::PlaceUnit) {
        const auto* e=project.catalog.find(placementArchetype);
        if(!e || e->type!="Unit")return false;
        item.kind="Unit";item.catalogId=e->id;item.name=e->id;
    }else if(tool==Tool::PlaceDoodad) {
        const auto* e=project.catalog.find(placementDoodad);
        if(!e || (e->type!="Actor" && e->type!="Model"))return false;
        item.kind="Doodad";item.catalogId=e->id;item.name=e->id;
    }else return false;
    rememberPlacement(project);
    project.placements.push_back(std::move(item));
    modified=true;return true;
}
bool EditorSession::removeNearest(RTSProject& project,const Vec3& world,float radius) {
    if(panel!=Panel::Terrain)return false;
    auto i=project.placements.end();
    float closest=radius;
    for(auto p=project.placements.begin();p!=project.placements.end();++p) {
        const float d=(p->position-world).lengthXZ();
        if(d<closest){closest=d;i=p;}
    }
    if(i==project.placements.end())return false;
    rememberPlacement(project);
    project.placements.erase(i);
    modified=true;return true;
}
const CatalogEntry* EditorSession::selected(const RTSProject& project)const {
    const auto& e=project.catalog.entries();
    if(e.empty())return nullptr;
    return &e[selectedObject%e.size()];
}
std::vector<std::string> EditorSession::fields(const RTSProject& project)const {
    const auto* e=selected(project);
    if(!e)return {};
    auto resolved=project.catalog.resolvedFields(e->id);
    std::vector<std::string> result;
    result.reserve(resolved.size());
    for(const auto& f:resolved)result.push_back(f.first);
    return result;
}
std::string EditorSession::selectedFieldName(const RTSProject& project)const {
    auto keys=fields(project);
    if(keys.empty())return "";
    return keys[selectedField%keys.size()];
}
bool EditorSession::catalogMatches(const CatalogEntry& entry) const {
    if(!catalogTypeFilter.empty() && entry.type!=catalogTypeFilter)return false;
    if(catalogFilter.empty())return true;
    auto lower=[](std::string text){
        for(auto& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));
        return text;
    };
    const auto query=lower(catalogFilter);
    if(lower(entry.id).find(query)!=std::string::npos ||
       lower(entry.type).find(query)!=std::string::npos)return true;
    auto name=entry.fields.find("name");
    return name!=entry.fields.end() && name->second.is<std::string>() &&
           lower(name->second.get<std::string>()).find(query)!=std::string::npos;
}
std::size_t EditorSession::matchingCatalogCount(const RTSProject& project)const {
    std::size_t count=0;
    for(const auto& entry:project.catalog.entries())if(catalogMatches(entry))++count;
    return count;
}
void EditorSession::beginCatalogSearch(){
    if(textEditing)return;
    filterEditing=true;textEditing=true;newFieldEditing=false;renamingCatalog=false;
    editText=catalogFilter;
    message="TYPE CATALOG SEARCH AND PRESS ENTER";
}
bool EditorSession::finishCatalogSearch(const RTSProject& project){
    if(!textEditing || !filterEditing)return false;
    const std::string old=catalogFilter;
    catalogFilter=editText.substr(0,128);
    filterEditing=false;textEditing=false;editText.clear();
    const auto count=matchingCatalogCount(project);
    if(count==0){
        catalogFilter=old;
        message="NO MATCHES - PREVIOUS FILTER KEPT";
        return false;
    }
    for(std::size_t i=0;i<project.catalog.entries().size();++i)
        if(catalogMatches(project.catalog.entries()[i])){
            selectedObject=i;break;
        }
    selectedField=0;
    message="MATCHES "+std::to_string(count);
    return true;
}
void EditorSession::clearCatalogSearch(const RTSProject& project){
    catalogFilter.clear();filterEditing=false;textEditing=false;editText.clear();
    if(selectedObject>=project.catalog.entries().size())selectedObject=0;
    message="CATALOG FILTER CLEARED";
}

bool EditorSession::setCatalogTypeFilter(const RTSProject& project,const std::string& type) {
    if(textEditing)return false;
    const auto& entries=project.catalog.entries();
    if(!type.empty() && std::none_of(entries.begin(),entries.end(),
       [&](const CatalogEntry& e){return e.type==type;}))return false;
    catalogTypeFilter=type;
    // Switching tabs should never strand users in a blank filtered tree.
    if(matchingCatalogCount(project)==0)catalogFilter.clear();
    for(std::size_t i=0;i<entries.size();++i){
        if(catalogMatches(entries[i])){
            if(selectedObject>=entries.size() || !catalogMatches(entries[selectedObject])){
                selectedObject=i;selectedField=0;
            }
            return true;
        }
    }
    selectedObject=0;selectedField=0;
    return true;
}
bool EditorSession::selectCatalogId(const RTSProject& project,const std::string& id) {
    if(textEditing)return false;
    const auto& entries=project.catalog.entries();
    for(std::size_t i=0;i<entries.size();++i){
        if(entries[i].id!=id)continue;
        // Linked navigation can cross catalog families and search filters.
        if(!catalogTypeFilter.empty() && catalogTypeFilter!=entries[i].type)
            catalogTypeFilter=entries[i].type;
        if(!catalogMatches(entries[i]))catalogFilter.clear();
        selectedObject=i;
        selectedField=0;
        message="OPENED "+id;
        return true;
    }
    return false;
}
bool EditorSession::setCatalogField(RTSProject& project,const std::string& key,
                                    const picojson::value& value) {
    if(textEditing)return false;
    const auto* selectedEntry=selected(project);
    if(!selectedEntry || key.empty())return false;
    const std::string id=selectedEntry->id;
    auto* entry=project.catalog.find(id);
    if(!entry)return false;
    const auto it=entry->fields.find(key);
    const bool had=it!=entry->fields.end();
    const picojson::value previous=had?it->second:picojson::value();
    if(had && previous.serialize()==value.serialize())return false;
    const RTSProject before=project;
    if(!project.catalog.setField(id,key,value)){
        message="INVALID PROPERTY KEY";return false;
    }
    try{project.validate();}
    catch(const std::exception& e){
        // Restore precisely the old local field: inherited values must not
        // become accidental overrides on validation failure.
        if(had)entry->fields[key]=previous;
        else entry->fields.erase(key);
        message=std::string("INVALID EDIT: ")+e.what();
        return false;
    }
    remember(before);
    modified=true;
    message="FIELD SAVED: "+key;
    const auto keys=fields(project);
    const auto found=std::find(keys.begin(),keys.end(),key);
    if(found!=keys.end())
        selectedField=static_cast<std::size_t>(found-keys.begin());
    return true;
}
void EditorSession::cycleObject(const RTSProject& project,int delta) {
    if(textEditing || project.catalog.entries().empty())return;
    const auto& entries=project.catalog.entries();
    const int n=int(entries.size());
    int current=int(selectedObject%entries.size());
    const int direction=delta<0?-1:1;
    for(int k=0;k<n;++k){
        current=(current+direction+n)%n;
        if(catalogMatches(entries[static_cast<std::size_t>(current)])){
            selectedObject=std::size_t(current);
            selectedField=0;
            return;
        }
    }
}
void EditorSession::cycleField(const RTSProject& project,int delta) {
    if(textEditing)return;
    auto v=fields(project);if(v.empty())return;
    int count=int(v.size());
    selectedField=static_cast<std::size_t>((int(selectedField%count)+delta%count+count)%count);
}
bool EditorSession::beginValueEdit(const RTSProject& project) {
    if(textEditing)return false;
    const auto* e=selected(project);if(!e)return false;
    std::string key=selectedFieldName(project);
    if(key.empty())return false;
    auto props=project.catalog.resolvedFields(e->id);
    editText=props.at(key).serialize();
    textEditing=true;newFieldEditing=false;renamingCatalog=false;
    message="ENTER TO APPLY FIELD / ESC TO CANCEL";
    return true;
}
void EditorSession::beginNewField(){
    if(textEditing)return;
    textEditing=true;newFieldEditing=true;renamingCatalog=false;pendingNewField_.clear();editText.clear();
    message="TYPE NEW PROPERTY NAME THEN ENTER";
}
void EditorSession::addChar(unsigned int c) {
    if(textEditing && c>=32 && c<=126 && editText.size()<768)
        editText.push_back(static_cast<char>(c));
}
void EditorSession::backspace(){
    if(textEditing && !editText.empty())editText.pop_back();
}
void EditorSession::cancelEdit(){
    textEditing=false;newFieldEditing=false;filterEditing=false;renamingCatalog=false;
    editText.clear();pendingNewField_.clear();
    message="EDIT CANCELLED";
}
bool EditorSession::finishEdit(RTSProject& project) {
    if(!textEditing)return false;
    if(newFieldEditing) {
        if(editText.empty())return false;
        pendingNewField_=editText;editText="null";newFieldEditing=false;
        message="ENTER JSON VALUE FOR "+pendingNewField_;
        return true;
    }
    const auto& entries=project.catalog.entries();
    if(entries.empty())return false;
    auto* selectedEntry=project.catalog.find(entries[selectedObject%entries.size()].id);
    if(!selectedEntry)return false;
    std::string key=pendingNewField_.empty()?selectedFieldName(project):pendingNewField_;
    if(key.empty())return false;
    const RTSProject originalProject=project;
    auto i=selectedEntry->fields.find(key);
    const bool had=i!=selectedEntry->fields.end();
    const picojson::value before=had?i->second:picojson::value();
    picojson::value next;
    const std::string parseError=picojson::parse(next,editText);
    if(!parseError.empty())next=picojson::value(editText); // unquoted text => string
    if(!project.catalog.setField(selectedEntry->id,key,next)) {
        message="INVALID PROPERTY KEY";return false;
    }
    try{project.validate();}
    catch(const std::exception& e) {
        if(had)selectedEntry->fields[key]=before;
        else selectedEntry->fields.erase(key);
        message=std::string("INVALID EDIT: ")+e.what();
        return false;
    }
    remember(originalProject);
    modified=true;textEditing=false;editText.clear();pendingNewField_.clear();
    message="FIELD SAVED";
    auto keys=fields(project);
    auto found=std::find(keys.begin(),keys.end(),key);
    if(found!=keys.end())selectedField=static_cast<std::size_t>(found-keys.begin());
    return true;
}
bool EditorSession::removeLocalOverride(RTSProject& project) {
    const auto* e=selected(project);if(!e)return false;
    auto key=selectedFieldName(project);
    auto* mutableEntry=project.catalog.find(e->id);
    if(!mutableEntry || !mutableEntry->fields.count(key))return false;
    const RTSProject before=project;
    auto old=mutableEntry->fields.at(key);
    mutableEntry->fields.erase(key);
    try{project.validate();}
    catch(...){mutableEntry->fields[key]=old;return false;}
    remember(before);
    modified=true;message="OVERRIDE REMOVED";return true;
}
bool EditorSession::createCatalogObject(RTSProject& project,
                                        const std::string& type,
                                        const std::string& preset){
    if(textEditing)return false;
    CatalogEntry entry;
    entry.type=type;
    try {
        if(type=="Unit"){
            const bool worker=preset=="Worker";
            const bool structure=preset=="Structure";
            if(!worker && !structure && preset!="Combat")return false;
            entry.id=worker?"new_worker":structure?"new_structure":"new_unit";
            entry.fields={
                {"name",picojson::value(worker?"New Worker":structure?"New Structure":"New Unit")},
                {"max_hp",picojson::value(structure?300.0:worker?35.0:65.0)},
                {"hp_regen",picojson::value(0.0)},
                {"max_energy",picojson::value(worker?0.0:100.0)},
                {"energy_regen",picojson::value(worker?0.0:1.5)},
                {"damage",picojson::value(structure?0.0:worker?1.0:8.0)},
                {"armor",picojson::value(0.0)},
                {"attack_range",picojson::value(structure?0.0:5.0)},
                {"move_speed",picojson::value(structure?0.0:8.0)},
                {"mineral_cost",picojson::value(structure?150.0:worker?35.0:60.0)},
                {"supply_cost",picojson::value(structure?0.0:1.0)},
                {"is_structure",picojson::value(structure)},
                {"is_worker",picojson::value(worker)},
                {"abilities",picojson::value(picojson::array{})}
            };
            for(const auto& object:project.catalog.entries())
                if(object.type=="Actor"){
                    entry.fields["actor"]=picojson::value(object.id);
                    break;
                }
        }else if(type=="Model" && preset=="Procedural"){
            entry.id="new_model";
            entry.fields={
                {"mesh",picojson::value("procedural.unit")},
                {"scale",picojson::value(picojson::array{
                    picojson::value(1.0),picojson::value(1.0),picojson::value(1.0)
                })}
            };
        }else if(type=="Actor" && (preset=="Visual" || preset=="Events")){
            entry.id="new_actor";
            entry.fields={
                {"tint",picojson::value(picojson::array{
                    picojson::value(0.7),picojson::value(0.8),picojson::value(1.0)
                })},
                {"opacity",picojson::value(1.0)}
            };
            if(preset=="Events"){
                entry.fields["events"]=picojson::value(picojson::array{
                    picojson::value(picojson::object{
                        {"when",picojson::value("Attack")},
                        {"style",picojson::value("Pulse")},
                        {"tint",picojson::value(picojson::array{
                            picojson::value(.2),picojson::value(.8),picojson::value(1.)})},
                        {"radius",picojson::value(1.5)},
                        {"duration_ticks",picojson::value(36.)}
                    })
                });
            }
            for(const auto& object:project.catalog.entries())
                if(object.type=="Model"){
                    entry.fields["model"]=picojson::value(object.id);
                    break;
                }
        }else if(type=="Upgrade"){
            if(preset!="Damage" && preset!="Armor")return false;
            entry.id="new_upgrade";
            entry.fields={{"name",picojson::value(
                              preset=="Damage"?"Sharpened Arms":"Layered Plating")},
                          {"max_level",picojson::value(3.)},
                          {"mineral_cost",picojson::value(75.)},
                          {"research_ticks",picojson::value(120.)},
                          {"damage_per_level",picojson::value(
                              preset=="Damage"?2.:0.)},
                          {"armor_per_level",picojson::value(
                              preset=="Armor"?1.:0.)}};
        }else if(type=="Validator"){
            entry.id="new_validator";
            if(preset!="Health" && preset!="Energy" && preset!="Status" &&
               preset!="Structure" && preset!="UnitCount" &&
               preset!="Upgrade")return false;
            const std::string kind=preset=="Health"?"HealthBelowPercent":
                preset=="Energy"?"EnergyAtLeast":
                preset=="Status"?"HasBehavior":
                preset=="Structure"?"IsStructure":
                preset=="Upgrade"?"UpgradeLevelAtLeast":"OwnerUnitCountAtLeast";
            entry.fields={{"kind",picojson::value(kind)},
                          {"subject",picojson::value(
                              preset=="Energy" || preset=="UnitCount" ||
                              preset=="Upgrade"?"Caster":"Target")},
                          {"value",picojson::value(
                              preset=="Health"?50.:preset=="Energy"?25.:1.)}};
            if(preset=="Upgrade"){
                for(const auto& object:project.catalog.entries())
                    if(object.type=="Upgrade"){
                        entry.fields["upgrade"]=picojson::value(object.id);
                        break;
                    }
                if(!entry.fields.count("upgrade"))
                    throw std::invalid_argument("Create an Upgrade first");
            }
            if(preset=="Status"){
                for(const auto& object:project.catalog.entries())
                    if(object.type=="Behavior"){
                        entry.fields["behavior"]=picojson::value(object.id);
                        break;
                    }
                if(!entry.fields.count("behavior"))
                    throw std::invalid_argument("Create a Behavior first");
            }
        }else if(type=="Requirement"){
            entry.id="new_requirement";
            if(preset!="Predicate" && preset!="All" &&
               preset!="Any" && preset!="Not")return false;
            entry.fields["kind"]=picojson::value(preset);
            const std::string targetType=preset=="Predicate"?
                "Validator":"Requirement";
            std::string refId;
            for(const auto& item:project.catalog.entries())
                if(item.type==targetType){refId=item.id;break;}
            if(refId.empty())throw std::invalid_argument(
                "Create a "+targetType+" first");
            if(preset=="Predicate")
                entry.fields["validator"]=picojson::value(refId);
            else entry.fields["children"]=picojson::value(
                picojson::array{picojson::value(refId)});
        }else if(type=="Weapon"){
            if(preset!="Ranged" && preset!="Melee")return false;
            entry.id="new_weapon";
            entry.fields={
                {"name",picojson::value(preset=="Ranged"?"Pulse Rifle":"Impact Blade")},
                {"damage",picojson::value(preset=="Ranged"?8.:12.)},
                {"range",picojson::value(preset=="Ranged"?7.:1.6)},
                {"cooldown",picojson::value(preset=="Ranged"?.8:1.2)},
                {"armor_piercing",picojson::value(preset=="Ranged"?1.:2.)}
            };
        }else if(type=="Projectile"){
            if(preset!="Arc" && preset!="Bolt")return false;
            entry.id="new_projectile";
            entry.fields={
                {"name",picojson::value(preset=="Arc"?"Arc Spark":"Pulse Bolt")},
                {"speed",picojson::value(preset=="Arc"?8.0:20.0)},
                {"arc_height",picojson::value(preset=="Arc"?3.0:0.0)},
                {"visual_scale",picojson::value(preset=="Arc"?0.65:0.40)},
                {"collision_radius",picojson::value(0.40)},
                {"collide_units",picojson::value(preset=="Bolt")},
                {"tint",picojson::value(picojson::array{
                    picojson::value(preset=="Arc"?0.20:1.0),
                    picojson::value(preset=="Arc"?0.85:0.50),
                    picojson::value(preset=="Arc"?1.0:0.20)})}
            };
        }else if(type=="Behavior"){
            if(preset!="Fortify" && preset!="Sharpen" && preset!="Hinder")
                return false;
            entry.id="new_behavior";
            entry.fields={
                {"name",picojson::value(preset=="Fortify"?"Protective Field":
                                         preset=="Sharpen"?"Battle Focus":"Tether Field")},
                {"duration_ticks",picojson::value(180.0)},
                {"max_stacks",picojson::value(preset=="Fortify"?3.0:
                                             preset=="Sharpen"?2.0:1.0)},
                {"armor_bonus",picojson::value(preset=="Fortify"?2.0:0.0)},
                {"damage_bonus",picojson::value(preset=="Sharpen"?4.0:0.0)},
                {"speed_multiplier",picojson::value(preset=="Hinder"?0.65:1.0)},
                {"refresh_duration",picojson::value(true)},
                {"dispellable",picojson::value(true)}
            };
        }else if(type=="Ability"){
            if(preset!="Self" && preset!="Ally" &&
               preset!="Enemy" && preset!="Any" && preset!="Ground")return false;
            entry.id="new_ability";
            entry.fields={
                {"name",picojson::value("New Ability")},
                {"target",picojson::value(preset)},
                {"cooldown_ticks",picojson::value(180.0)},
                {"cast_range",picojson::value(preset=="Self"?0.0:
                                            preset=="Ground"?12.0:9.0)},
                {"mineral_cost",picojson::value(0.0)},
                {"energy_cost",picojson::value(0.0)}
            };
            if(preset=="Ground"){
                entry.fields["area_radius"]=picojson::value(4.0);
                entry.fields["area_team"]=picojson::value(std::string("Enemy"));
                entry.fields["impact_delay_ticks"]=picojson::value(60.0);
            }
            for(const auto& object:project.catalog.entries())
                if(object.type=="Effect"){
                    entry.fields["effect"]=picojson::value(object.id);
                    break;
                }
            if(!entry.fields.count("effect"))
                throw std::invalid_argument("Create an Effect before an Ability");
        }else if(type=="Effect"){
            const bool periodic=preset=="PeriodicDamage" || preset=="PeriodicHeal";
            const bool behavior=preset=="ApplyBehavior" ||
                                preset=="RemoveBehavior";
            if(preset!="Damage" && preset!="Heal" && !periodic &&
               preset!="Sequence" && !behavior)return false;
            entry.id="new_effect";
            entry.fields["kind"]=picojson::value(preset);
            if(behavior){
                for(const auto& object:project.catalog.entries())
                    if(object.type=="Behavior"){
                        entry.fields["behavior"]=picojson::value(object.id);
                        break;
                    }
                if(!entry.fields.count("behavior"))
                    throw std::invalid_argument("Create a Behavior before a behavior Effect");
            }else if(preset=="Sequence"){
                for(const auto& object:project.catalog.entries())
                    if(object.type=="Effect"){
                        entry.fields["children"]=picojson::value(
                            picojson::array{picojson::value(object.id)});
                        break;
                    }
                if(!entry.fields.count("children"))
                    throw std::invalid_argument("Create a leaf Effect before a Sequence");
            }else{
                entry.fields["amount"]=picojson::value(8.0);
                if(periodic){
                    entry.fields["interval_ticks"]=picojson::value(60.0);
                    entry.fields["duration_ticks"]=picojson::value(240.0);
                }
            }
        }else return false;

        const std::string base=entry.id;
        for(int i=2;project.catalog.find(entry.id);++i){
            if(i>10000)throw std::invalid_argument("No available catalog ID");
            entry.id=base+std::to_string(i);
        }
        RTSProject candidate=project;
        std::string reason;
        if(!candidate.catalog.add(entry,&reason))
            throw std::invalid_argument(reason.empty()?"Unable to create catalog object":reason);
        candidate.validate();
        (void)parseTriggerRules(candidate);
        remember(project);
        project=std::move(candidate);
        selectedObject=project.catalog.entries().size()-1;
        selectedField=0;
        modified=true;
        message="CREATED "+entry.type+" "+entry.id;
        return true;
    }catch(const std::exception& e){
        message=std::string("CATALOG OBJECT REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setActorCues(
    RTSProject& project,const std::vector<RTSActorCueDefinition>& values){
    if(textEditing)return false;
    const auto* actor=selected(project);
    if(!actor || actor->type!="Actor")return false;
    try{
        RTSProject next=project;
        picojson::array events;
        events.reserve(values.size());
        for(const auto& cue:values){
            picojson::object item;
            item["when"]=picojson::value(rtsActorCueEventName(cue.event));
            item["style"]=picojson::value(rtsActorCueStyleName(cue.style));
            item["tint"]=picojson::value(picojson::array{
                picojson::value(double(cue.tint.x)),
                picojson::value(double(cue.tint.y)),
                picojson::value(double(cue.tint.z))});
            item["radius"]=picojson::value(double(cue.radius));
            item["duration_ticks"]=picojson::value(double(cue.durationTicks));
            events.push_back(picojson::value(item));
        }
        if(!next.catalog.setField(actor->id,"events",picojson::value(events)))
            throw std::invalid_argument("Cannot edit Actor event rules");
        next.validate();
        (void)parseTriggerRules(next);
        if(next.toJson().serialize()==project.toJson().serialize())return false;
        const std::string selectedId=actor->id;
        remember(project);
        project=std::move(next);
        modified=true;message="ACTOR CUES SAVED: "+selectedId;
        return true;
    }catch(const std::exception& e){
        message=std::string("INVALID ACTOR CUES: ")+e.what();
        return false;
    }
}
bool EditorSession::setUpgradeDefinition(
    RTSProject& project,const RTSUpgradeDefinition& values){
    if(textEditing)return false;
    const auto* selectedUpgrade=selected(project);
    if(!selectedUpgrade || selectedUpgrade->type!="Upgrade" ||
       selectedUpgrade->id!=values.id)return false;
    try{
        RTSProject next=project;
        auto* entry=next.catalog.find(values.id);
        auto& fields=entry->fields;
        fields["max_level"]=picojson::value(double(values.maxLevel));
        fields["mineral_cost"]=picojson::value(double(values.mineralCost));
        fields["research_ticks"]=picojson::value(double(values.researchTicks));
        fields["damage_per_level"]=picojson::value(double(values.damagePerLevel));
        fields["armor_per_level"]=picojson::value(double(values.armorPerLevel));
        fields["unit"]=picojson::value(values.unitId);
        fields["requirement"]=picojson::value(values.requirementId);
        next.validate();
        (void)parseTriggerRules(next);
        if(next.toJson().serialize()==project.toJson().serialize())return false;
        remember(project);project=std::move(next);
        modified=true;message="UPGRADE SAVED: "+values.id;
        return true;
    }catch(const std::exception& e){
        message=std::string("UPGRADE REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setValidatorDefinition(
    RTSProject& project,const RTSValidatorDefinition& values){
    if(textEditing)return false;
    const auto* current=selected(project);
    if(!current || current->type!="Validator" || current->id!=values.id)
        return false;
    try{
        RTSProject candidate=project;
        auto* item=candidate.catalog.find(values.id);
        auto& f=item->fields;
        f["kind"]=picojson::value(rtsValidatorKindName(values.kind));
        f["subject"]=picojson::value(rtsValidatorSubjectName(values.subject));
        f["value"]=picojson::value(double(values.value));
        f["behavior"]=picojson::value(values.behaviorId);
        f["unit"]=picojson::value(values.unitId);
        f["upgrade"]=picojson::value(values.upgradeId);
        candidate.validate();
        (void)parseTriggerRules(candidate);
        if(candidate.toJson().serialize()==project.toJson().serialize())return false;
        remember(project);project=std::move(candidate);
        modified=true;message="VALIDATOR SAVED: "+values.id;
        return true;
    }catch(const std::exception& e){
        message=std::string("VALIDATOR REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setRequirementDefinition(
    RTSProject& project,RTSRequirementKind kind,
    const std::string& validatorId,const std::vector<std::string>& children){
    if(textEditing)return false;
    const auto* current=selected(project);
    if(!current || current->type!="Requirement")return false;
    try{
        RTSProject candidate=project;
        auto* item=candidate.catalog.find(current->id);
        auto& f=item->fields;
        f["kind"]=picojson::value(rtsRequirementKindName(kind));
        f["validator"]=picojson::value(validatorId);
        picojson::array refs;
        for(const auto& child:children)refs.push_back(picojson::value(child));
        f["children"]=picojson::value(refs);
        candidate.validate();
        (void)parseTriggerRules(candidate);
        if(candidate.toJson().serialize()==project.toJson().serialize())return false;
        const std::string id=current->id;
        remember(project);project=std::move(candidate);
        modified=true;message="REQUIREMENT SAVED: "+id;
        return true;
    }catch(const std::exception& e){
        message=std::string("REQUIREMENT REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setWeaponDefinition(
    RTSProject& project,const WeaponDefinition& values){
    if(textEditing)return false;
    const auto* current=selected(project);
    if(!current || current->type!="Weapon" || current->id!=values.id)
        return false;
    try{
        RTSProject candidate=project;
        auto* weapon=candidate.catalog.find(values.id);
        if(!weapon)throw std::invalid_argument("Missing Weapon");
        auto& f=weapon->fields;
        f["damage"]=picojson::value(double(values.damage));
        f["range"]=picojson::value(double(values.range));
        f["cooldown"]=picojson::value(double(values.cooldown));
        f["armor_piercing"]=picojson::value(double(values.armorPiercing));
        f["projectile"]=picojson::value(values.projectileId);
        candidate.validate();(void)parseTriggerRules(candidate);
        if(candidate.toJson().serialize()==project.toJson().serialize())
            return false;
        remember(project);
        project=std::move(candidate);
        modified=true;
        message="WEAPON SETTINGS SAVED: "+values.id;
        return true;
    }catch(const std::exception& e){
        message=std::string("WEAPON REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setProjectileDefinition(
    RTSProject& project,const ProjectileDefinition& values){
    if(textEditing)return false;
    const auto* current=selected(project);
    if(!current || current->type!="Projectile" || current->id!=values.id)
        return false;
    try{
        RTSProject candidate=project;
        auto* projectile=candidate.catalog.find(values.id);
        if(!projectile)throw std::invalid_argument("Missing Projectile");
        auto& f=projectile->fields;
        f["speed"]=picojson::value(double(values.speed));
        f["arc_height"]=picojson::value(double(values.arcHeight));
        f["visual_scale"]=picojson::value(double(values.visualScale));
        f["collision_radius"]=picojson::value(double(values.collisionRadius));
        f["collide_units"]=picojson::value(values.collideUnits);
        f["tint"]=picojson::value(picojson::array{
            picojson::value(double(values.tint.x)),
            picojson::value(double(values.tint.y)),
            picojson::value(double(values.tint.z))});
        candidate.validate();
        (void)parseTriggerRules(candidate);
        if(candidate.toJson().serialize()==project.toJson().serialize())
            return false;
        remember(project);
        project=std::move(candidate);
        modified=true;
        message="PROJECTILE SETTINGS SAVED: "+values.id;
        return true;
    }catch(const std::exception& e){
        message=std::string("PROJECTILE REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setAbilityTargetPolicy(
    RTSProject& project,const std::string& target){
    if(textEditing)return false;
    if(target!="Self" && target!="Ally" && target!="Enemy" &&
       target!="Any" && target!="Ground")return false;
    const auto* selectedAbility=selected(project);
    if(!selectedAbility || selectedAbility->type!="Ability")return false;
    try{
        RTSProject candidate=project;
        auto* ability=candidate.catalog.find(selectedAbility->id);
        const auto fields=candidate.catalog.resolvedFields(ability->id);
        const auto it=fields.find("target");
        if(it!=fields.end() && it->second.is<std::string>() &&
           it->second.get<std::string>()==target)return false;
        ability->fields["target"]=picojson::value(target);
        // The Ability may inherit a Projectile from its parent. A local
        // empty-string override is required to neutralize that inherited
        // reference when the target type stops supporting projectiles.
        if(target!="Ground")
            ability->fields["projectile"]=picojson::value(std::string{});
        const auto priorRange=fields.find("cast_range");
        const double range=priorRange!=fields.end() &&
            priorRange->second.is<double>()?priorRange->second.get<double>():0.;
        if(target=="Ground"){
            if(range<.5)ability->fields["cast_range"]=picojson::value(12.0);
            ability->fields["area_radius"]=picojson::value(4.0);
            ability->fields["area_team"]=picojson::value(std::string("Enemy"));
            ability->fields["area_shape"]=picojson::value(std::string("Circle"));
            ability->fields["area_angle"]=picojson::value(90.0);
            ability->fields["area_width"]=picojson::value(2.0);
            ability->fields["impact_delay_ticks"]=picojson::value(60.0);
        }else if(target!="Self" && range<.5)
            ability->fields["cast_range"]=picojson::value(9.0);
        candidate.validate();(void)parseTriggerRules(candidate);
        remember(project);project=std::move(candidate);
        modified=true;message="ABILITY TARGET: "+target;
        return true;
    }catch(const std::exception& e){
        message=std::string("ABILITY TARGET REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setGroundAbilitySettings(
    RTSProject& project,float radius,const std::string& team,int delayTicks,
    const std::string& shape,float angle,float width){
    if(textEditing)return false;
    const auto* current=selected(project);
    if(!current || current->type!="Ability" ||
       compileAbility(project.catalog,current->id).target!=AbilityTarget::Ground)
        return false;
    try{
        RTSProject candidate=project;
        auto* ability=candidate.catalog.find(current->id);
        ability->fields["area_radius"]=picojson::value(double(radius));
        ability->fields["area_team"]=picojson::value(team);
        ability->fields["area_shape"]=picojson::value(shape);
        ability->fields["area_angle"]=picojson::value(double(angle));
        ability->fields["area_width"]=picojson::value(double(width));
        ability->fields["impact_delay_ticks"]=picojson::value(double(delayTicks));
        candidate.validate();(void)parseTriggerRules(candidate);
        if(candidate.toJson().serialize()==project.toJson().serialize())return false;
        remember(project);project=std::move(candidate);
        modified=true;message="GROUND ABILITY SETTINGS SAVED";
        return true;
    }catch(const std::exception& e){
        message=std::string("GROUND ABILITY REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::setBehaviorDefinition(
    RTSProject& project,const BehaviorDefinition& values){
    if(textEditing)return false;
    const auto* current=selected(project);
    if(!current || current->type!="Behavior" || values.id!=current->id)
        return false;
    try{
        RTSProject candidate=project;
        auto* behavior=candidate.catalog.find(values.id);
        if(!behavior)throw std::invalid_argument("Missing edited Behavior");
        auto& fields=behavior->fields;
        fields["duration_ticks"]=picojson::value(double(values.durationTicks));
        fields["max_stacks"]=picojson::value(double(values.maxStacks));
        fields["armor_bonus"]=picojson::value(double(values.armorBonus));
        fields["damage_bonus"]=picojson::value(double(values.damageBonus));
        fields["speed_multiplier"]=picojson::value(double(values.speedMultiplier));
        fields["refresh_duration"]=picojson::value(values.refreshDuration);
        fields["dispellable"]=picojson::value(values.dispellable);
        candidate.validate();
        (void)parseTriggerRules(candidate);
        if(candidate.toJson().serialize()==project.toJson().serialize())return false;
        remember(project);
        project=std::move(candidate);
        modified=true;
        message="BEHAVIOR PARAMETERS SAVED: "+values.id;
        return true;
    }catch(const std::exception& e){
        message=std::string("BEHAVIOR REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::convertEffectKind(RTSProject& project,
                                      const std::string& kind){
    if(textEditing)return false;
    if(kind!="Damage" && kind!="Heal" && kind!="PeriodicDamage" &&
       kind!="PeriodicHeal" && kind!="Sequence" &&
       kind!="ApplyBehavior" && kind!="RemoveBehavior")return false;
    const auto* current=selected(project);
    if(!current || current->type!="Effect")return false;
    RTSProject candidate=project;
    try{
        auto* effect=candidate.catalog.find(current->id);
        if(!effect)throw std::invalid_argument("Selected Effect no longer exists");
        auto& fields=effect->fields;
        const auto resolved=candidate.catalog.resolvedFields(effect->id);
        const auto found=resolved.find("kind");
        if(found!=resolved.end() && found->second.is<std::string>() &&
           found->second.get<std::string>()==kind)return false;
        fields["kind"]=picojson::value(kind);
        if(kind=="ApplyBehavior" || kind=="RemoveBehavior"){
            const auto ref=resolved.find("behavior");
            const std::string existing=ref!=resolved.end() &&
                ref->second.is<std::string>()?ref->second.get<std::string>():"";
            const auto* linked=candidate.catalog.find(existing);
            if(!linked || linked->type!="Behavior"){
                bool found=false;
                for(const auto& object:candidate.catalog.entries())
                    if(object.type=="Behavior"){
                        fields["behavior"]=picojson::value(object.id);
                        found=true;break;
                    }
                if(!found)throw std::invalid_argument("Create a Behavior first");
            }
        }else if(kind=="Sequence"){
            const auto refs=resolved.find("children");
            if(refs==resolved.end() || !refs->second.is<picojson::array>() ||
               refs->second.get<picojson::array>().empty()){
                bool foundOther=false;
                for(const auto& object:candidate.catalog.entries())
                    if(object.type=="Effect" && object.id!=effect->id){
                        fields["children"]=picojson::value(
                            picojson::array{picojson::value(object.id)});
                        foundOther=true;break;
                    }
                if(!foundOther)
                    throw std::invalid_argument("A Sequence needs another Effect first");
            }
        }else{
            const auto amount=resolved.find("amount");
            if(amount==resolved.end() || !amount->second.is<double>() ||
               !std::isfinite(amount->second.get<double>()) ||
               amount->second.get<double>()<=0.0 ||
               amount->second.get<double>()>100000.0)
                fields["amount"]=picojson::value(8.0);
            if(kind=="PeriodicDamage" || kind=="PeriodicHeal"){
                const auto ensureTicks=[&](const char* key,double fallback,
                                           double maximum){
                    auto prior=resolved.find(key);
                    if(prior!=resolved.end() && prior->second.is<double>()){
                        const double n=prior->second.get<double>();
                        if(std::isfinite(n) && std::floor(n)==n &&
                           n>=1.0 && n<=maximum)return;
                    }
                    fields[key]=picojson::value(fallback);
                };
                ensureTicks("interval_ticks",60.0,600.0);
                ensureTicks("duration_ticks",240.0,3600.0);
            }
        }
        candidate.validate();
        (void)parseTriggerRules(candidate);
    }catch(const std::exception& e){
        message=std::string("EFFECT CONVERSION REJECTED: ")+e.what();
        return false;
    }
    remember(project);
    project=std::move(candidate);
    modified=true;
    message="EFFECT KIND CONVERTED TO "+kind;
    return true;
}
bool EditorSession::duplicateCatalogObject(RTSProject& project) {
    const auto* e=selected(project);if(!e)return false;
    const std::string base=e->id+"_copy";
    std::string next=base;
    int suffix=2;
    while(project.catalog.find(next))next=base+std::to_string(suffix++);
    const RTSProject before=project;
    if(!project.catalog.cloneDerived(e->id,next))return false;
    remember(before);
    selectedObject=project.catalog.entries().size()-1;
    selectedField=0;modified=true;message="DERIVED "+next;return true;
}

bool EditorSession::beginCatalogRename(const RTSProject& project) {
    if(textEditing)return false;
    const auto* object=selected(project);
    if(!object)return false;
    textEditing=true;renamingCatalog=true;filterEditing=false;newFieldEditing=false;
    editText=object->id;
    message="RENAME CATALOG ID / ENTER APPLY / ESC CANCEL";
    return true;
}
bool EditorSession::finishCatalogRename(RTSProject& project) {
    if(!textEditing || !renamingCatalog)return false;
    const auto* object=selected(project);
    if(!object)return false;
    const std::string oldId=object->id;
    if(oldId==editText) {
        renamingCatalog=false;textEditing=false;editText.clear();
        message="CATALOG ID UNCHANGED";return false;
    }
    const RTSProject before=project;
    try {
        if(!project.renameCatalogObject(oldId,editText)) {
            message="INVALID OR DUPLICATE CATALOG ID";return false;
        }
    }catch(const std::exception& e) {
        message=std::string("RENAME FAILED: ")+e.what();
        return false;
    }
    if(placementArchetype==oldId)placementArchetype=editText;
    if(placementDoodad==oldId)placementDoodad=editText;
    remember(before);
    renamingCatalog=false;textEditing=false;editText.clear();
    modified=true;message="RENAMED "+oldId;
    return true;
}
bool EditorSession::deleteCatalogObject(RTSProject& project) {
    if(textEditing)return false;
    const auto* object=selected(project);
    if(!object)return false;
    const std::string id=object->id;
    const RTSProject before=project;
    std::string why;
    if(!project.removeCatalogObject(id,&why)) {
        message="CANNOT DELETE "+id+": "+why;
        return false;
    }
    remember(before);
    if(placementArchetype==id || placementDoodad==id) {
        placementArchetype.clear();
        placementDoodad.clear();
        for(const auto& entry:project.catalog.entries()) {
            if(placementArchetype.empty() && entry.type=="Unit")placementArchetype=entry.id;
            if(placementDoodad.empty() && entry.type=="Actor")placementDoodad=entry.id;
        }
    }
    if(!project.catalog.entries().empty())
        selectedObject=std::min(selectedObject,project.catalog.entries().size()-1);
    else selectedObject=0;
    selectedField=0;
    if(matchingCatalogCount(project)==0){
        catalogFilter.clear();
        if(matchingCatalogCount(project)==0)catalogTypeFilter.clear();
    }
    modified=true;message="DELETED "+id;
    return true;
}

namespace {
const picojson::array& triggerArray(const RTSProject& project) {
    static const picojson::array empty;
    auto it=project.extensions.find("triggers");
    return (it!=project.extensions.end() && it->second.is<picojson::array>())
        ? it->second.get<picojson::array>() : empty;
}
picojson::array& mutableTriggers(RTSProject& project) {
    auto it=project.extensions.find("triggers");
    if(it==project.extensions.end())
        it=project.extensions.emplace("triggers",picojson::value(picojson::array())).first;
    if(!it->second.is<picojson::array>())
        throw std::invalid_argument("Project triggers must be JSON array");
    return it->second.get<picojson::array>();
}
std::string triggerId(const picojson::value& v) {
    if(!v.is<picojson::object>())return "<BAD RULE>";
    const auto& ob=v.get<picojson::object>();
    auto it=ob.find("id");
    return it!=ob.end() && it->second.is<std::string>()?
        it->second.get<std::string>():"<UNNAMED RULE>";
}
}
bool EditorSession::mutateTriggerVariables(RTSProject& project,
    const std::function<void(picojson::object&)>& change,
    const std::string& status){
    if(textEditing)return false;
    RTSProject candidate=project;
    try{
        auto it=candidate.extensions.find("trigger_variables");
        if(it==candidate.extensions.end())
            it=candidate.extensions.emplace("trigger_variables",
                picojson::value(picojson::object{})).first;
        if(!it->second.is<picojson::object>())
            throw std::invalid_argument("trigger_variables must be object");
        auto& variables=it->second.get<picojson::object>();
        const std::string before=picojson::value(variables).serialize();
        change(variables);
        if(before==picojson::value(variables).serialize())return false;
        (void)parseTriggerRules(candidate); // checks IDs, ranges, and live references
        candidate.validate();
    }catch(const std::exception& e){
        message=std::string("VARIABLE EDIT REJECTED: ")+e.what();
        return false;
    }
    remember(project);
    project=std::move(candidate);
    modified=true;
    message=status;
    return true;
}
bool EditorSession::addTriggerVariable(RTSProject& project,
                                       const std::string& name,int initial){
    return mutateTriggerVariables(project,[&](picojson::object& vars){
        if(vars.count(name))throw std::invalid_argument("Duplicate variable: "+name);
        vars.emplace(name,picojson::value(double(initial)));
    },"VARIABLE CREATED");
}
bool EditorSession::setTriggerVariable(RTSProject& project,
                                       const std::string& name,int value){
    return mutateTriggerVariables(project,[&](picojson::object& vars){
        auto it=vars.find(name);
        if(it==vars.end())throw std::invalid_argument("Unknown variable: "+name);
        it->second=picojson::value(double(value));
    },"VARIABLE INITIAL VALUE UPDATED");
}
bool EditorSession::removeTriggerVariable(RTSProject& project,
                                          const std::string& name){
    return mutateTriggerVariables(project,[&](picojson::object& vars){
        if(!vars.erase(name))throw std::invalid_argument("Unknown variable: "+name);
    },"VARIABLE REMOVED");
}
bool EditorSession::renameTriggerVariable(RTSProject& project,
                                          const std::string& oldName,
                                          const std::string& newName){
    if(textEditing || oldName==newName)return false;
    RTSProject candidate=project;
    try{
        auto globals=candidate.extensions.find("trigger_variables");
        if(globals==candidate.extensions.end() ||
           !globals->second.is<picojson::object>())
            throw std::invalid_argument("Missing trigger variables");
        auto& vars=globals->second.get<picojson::object>();
        auto original=vars.find(oldName);
        if(original==vars.end())throw std::invalid_argument("Unknown variable: "+oldName);
        if(vars.count(newName))throw std::invalid_argument("Variable already exists: "+newName);
        const picojson::value startingValue=original->second;
        vars.erase(original);
        vars.emplace(newName,startingValue);

        const auto updateReference=[&](picojson::object& node,
                                       const char* requiredKind){
            auto type=node.find("type");
            auto ref=node.find("variable");
            if(type!=node.end() && type->second.is<std::string>() &&
               type->second.get<std::string>()==requiredKind &&
               ref!=node.end() && ref->second.is<std::string>() &&
               ref->second.get<std::string>()==oldName)
                ref->second=picojson::value(newName);
        };
        // Migrate only typed expression nodes inside recognized ECA operands.
        // Unrelated extension data and user-authored text must be left intact.
        const auto updateExpression=[&](auto&& self,picojson::value& expr)->void {
            if(!expr.is<picojson::object>())return;
            auto& object=expr.get<picojson::object>();
            const auto op=object.find("op");
            if(op!=object.end() && op->second.is<std::string>() &&
               op->second.get<std::string>()=="Variable"){
                auto name=object.find("name");
                if(name!=object.end() && name->second.is<std::string>() &&
                   name->second.get<std::string>()==oldName)
                    name->second=picojson::value(newName);
            }
            for(const char* key:{"operand","left","right","value","minimum","maximum"}){
                auto child=object.find(key);
                if(child!=object.end())self(self,child->second);
            }
        };
        // Walk only recognized ECA tree nodes. Leave unrelated custom JSON
        // extensions and free-form text intact during dependency migrations.
        const auto updateNode=[&](auto&& self,picojson::object& node)->void {
            const auto t=node.find("type");
            if(t==node.end() || !t->second.is<std::string>())return;
            const auto& type=t->second.get<std::string>();
            if(type=="OnVariableChanged" || type=="VariableCompare" ||
               type=="SetVariable" || type=="AddVariable")
                updateReference(node,type.c_str());
            for(const char* exprKey:{"value_expr","amount_expr","times_expr"}){
                auto expression=node.find(exprKey);
                if(expression!=node.end())
                    updateExpression(updateExpression,expression->second);
            }
            if(type=="IfThenElse"){
                for(const char* key:{"conditions","then","else"}){
                    auto subtree=node.find(key);
                    if(subtree==node.end() || !subtree->second.is<picojson::array>())continue;
                    for(auto& child:subtree->second.get<picojson::array>())
                        if(child.is<picojson::object>())
                            self(self,child.get<picojson::object>());
                }
            }else if(type=="Repeat"){
                auto subtree=node.find("actions");
                if(subtree!=node.end() && subtree->second.is<picojson::array>())
                    for(auto& child:subtree->second.get<picojson::array>())
                        if(child.is<picojson::object>())
                            self(self,child.get<picojson::object>());
            }
        };
        auto rules=candidate.extensions.find("triggers");
        if(rules!=candidate.extensions.end() && rules->second.is<picojson::array>()){
            for(auto& item:rules->second.get<picojson::array>()){
                if(!item.is<picojson::object>())continue;
                auto& rule=item.get<picojson::object>();
                auto event=rule.find("event");
                if(event!=rule.end() && event->second.is<picojson::object>())
                    updateNode(updateNode,event->second.get<picojson::object>());
                for(const char* category:{"conditions","actions"}){
                    auto nodes=rule.find(category);
                    if(nodes==rule.end() || !nodes->second.is<picojson::array>())continue;
                    for(auto& child:nodes->second.get<picojson::array>())
                        if(child.is<picojson::object>())
                            updateNode(updateNode,child.get<picojson::object>());
                }
            }
        }
        (void)parseTriggerRules(candidate);
        candidate.validate();
    }catch(const std::exception& e){
        message=std::string("VARIABLE RENAME REJECTED: ")+e.what();
        return false;
    }
    remember(project);
    project=std::move(candidate);
    modified=true;
    message="VARIABLE REFERENCES RENAMED";
    return true;
}
std::size_t EditorSession::triggerCount(const RTSProject& project)const {
    return triggerArray(project).size();
}
std::string EditorSession::triggerLabel(const RTSProject& project,std::size_t index)const {
    const auto& a=triggerArray(project);
    if(index>=a.size())return "";
    const auto& value=a[index];
    std::string label=triggerId(value);
    if(value.is<picojson::object>()) {
        auto e=value.get<picojson::object>().find("event");
        if(e!=value.get<picojson::object>().end() && e->second.is<picojson::object>()){
            auto fields=e->second.get<picojson::object>();
            auto name=fields.find("type");
            if(name!=fields.end() && name->second.is<std::string>())
                label+=" ["+name->second.get<std::string>()+"]";
        }
    }
    return label;
}
std::string EditorSession::triggerJSON(const RTSProject& project,std::size_t index)const {
    const auto& a=triggerArray(project);
    return index<a.size()?a[index].serialize():"";
}
void EditorSession::cycleTrigger(const RTSProject& project,int delta) {
    if(textEditing)return;
    const auto count=triggerCount(project);
    if(count==0)return;
    const int n=int(count);
    selectedTrigger=static_cast<std::size_t>((int(selectedTrigger%count)+delta%n+n)%n);
}
bool EditorSession::createTrigger(RTSProject& project) {
    auto& a=mutableTriggers(project);
    if(a.size()>=256)return false;
    std::string id="new_rule";
    int suffix=2;
    auto exists=[&](const std::string& target){
        for(const auto& r:a)if(triggerId(r)==target)return true;
        return false;
    };
    while(exists(id))id="new_rule_"+std::to_string(suffix++);
    picojson::object event{{"type",picojson::value("OnMapStart")}};
    picojson::array actions{picojson::value(picojson::object{
        {"type",picojson::value("ShowMessage")},
        {"text",picojson::value("EDIT THIS TRIGGER")}})};
    picojson::object v{
        {"id",picojson::value(id)},{"event",picojson::value(event)},
        {"once",picojson::value(true)},{"actions",picojson::value(actions)}
    };
    const RTSProject before=project;
    a.emplace_back(v);
    remember(before);
    selectedTrigger=a.size()-1;
    modified=true;message="TRIGGER CREATED";
    return true;
}
bool EditorSession::duplicateTrigger(RTSProject& project) {
    auto& a=mutableTriggers(project);
    if(a.empty() || a.size()>=256)return false;
    const auto src=a[selectedTrigger%a.size()];
    picojson::object copied=src.get<picojson::object>();
    std::string candidate=triggerId(src)+"_copy";
    int n=2;
    auto exists=[&](const std::string& target){
        for(const auto& r:a)if(triggerId(r)==target)return true;
        return false;
    };
    std::string base=candidate;
    while(exists(candidate))candidate=base+std::to_string(n++);
    copied["id"]=picojson::value(candidate);
    const RTSProject before=project;
    a.emplace_back(copied);
    remember(before);
    selectedTrigger=a.size()-1;
    modified=true;message="TRIGGER DUPLICATED";
    return true;
}
bool EditorSession::deleteTrigger(RTSProject& project) {
    auto& a=mutableTriggers(project);
    if(a.empty())return false;
    const RTSProject before=project;
    a.erase(a.begin()+static_cast<std::ptrdiff_t>(selectedTrigger%a.size()));
    remember(before);
    selectedTrigger=std::min(selectedTrigger,a.empty()?std::size_t(0):a.size()-1);
    modified=true;message="TRIGGER REMOVED";
    return true;
}
picojson::object EditorSession::triggerObject(const RTSProject& project,std::size_t index) const {
    const auto& a=triggerArray(project);
    if(index>=a.size() || !a[index].is<picojson::object>())return {};
    return a[index].get<picojson::object>();
}
bool EditorSession::mutateTrigger(
    RTSProject& project,const std::function<void(picojson::object&)>& change,
    const std::string& status) {
    if(textEditing || triggerCount(project)==0)return false;
    RTSProject candidate=project;
    try{
        auto& a=mutableTriggers(candidate);
        auto& rule=a[selectedTrigger%a.size()].get<picojson::object>();
        const std::string original=a[selectedTrigger%a.size()].serialize();
        change(rule);
        if(a[selectedTrigger%a.size()].serialize()==original)return false;
        candidate.validate();
        (void)parseTriggerRules(candidate);
    }catch(const std::exception& e){
        message=std::string("TRIGGER REJECTED: ")+e.what();
        return false;
    }
    remember(project);
    project=std::move(candidate);
    modified=true;message=status;
    return true;
}
bool EditorSession::setTriggerEvent(RTSProject& project,const std::string& type) {
    if(type!="OnMapStart" && type!="After" && type!="Every" &&
       type!="OnUnitDeath" && type!="OnRegionEnter" && type!="MineralsBelow" &&
       type!="OnVariableChanged")return false;
    picojson::object event{{"type",picojson::value(type)}};
    if(type=="After" || type=="Every")event["seconds"]=picojson::value(1.0);
    if(type=="MineralsBelow")event["amount"]=picojson::value(100.0);
    if(type=="OnVariableChanged"){
        const auto globals=project.extensions.find("trigger_variables");
        if(globals==project.extensions.end() || !globals->second.is<picojson::object>() ||
           globals->second.get<picojson::object>().empty()){
            message="CREATE A TRIGGER VARIABLE FIRST";
            return false;
        }
        event["variable"]=picojson::value(
            globals->second.get<picojson::object>().begin()->first);
    }
    if(type=="OnRegionEnter") {
        auto it=std::find_if(project.placements.begin(),project.placements.end(),
            [](const ProjectPlacement& p){return p.kind=="Region";});
        if(it==project.placements.end()) {
            message="CREATE A REGION BEFORE USING OnRegionEnter";
            return false;
        }
        event["region"]=picojson::value(it->id);
    }
    return mutateTrigger(project,[&](picojson::object& rule) {
        rule["event"]=picojson::value(event);
        rule["once"]=picojson::value(type=="OnMapStart" || type=="After");
    },"TRIGGER EVENT UPDATED");
}
bool EditorSession::setTriggerOnce(RTSProject& project,bool once){
    return mutateTrigger(project,[&](picojson::object& rule){
        rule["once"]=picojson::value(once);
    },"TRIGGER REPETITION UPDATED");
}
bool EditorSession::setTriggerEventField(RTSProject& project,
    const std::string& key,const picojson::value& value){
    if(key=="type")return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        auto& event=rule.at("event").get<picojson::object>();
        if(!event.count(key))throw std::invalid_argument("Unknown event field");
        event[key]=value;
    },"EVENT FIELD UPDATED");
}
bool EditorSession::addTriggerNode(RTSProject& project,bool condition,const std::string& type){
    return addTriggerTreeNode(project,{},condition?"conditions":"actions",type);
}
bool EditorSession::removeTriggerNode(RTSProject& project,bool condition,std::size_t index){
    return mutateTrigger(project,[&](picojson::object& rule){
        auto& a=rule.at(condition?"conditions":"actions").get<picojson::array>();
        if(index>=a.size())throw std::invalid_argument("Node index out of range");
        a.erase(a.begin()+static_cast<std::ptrdiff_t>(index));
    },"TRIGGER NODE REMOVED");
}
bool EditorSession::moveTriggerNode(RTSProject& project,bool condition,std::size_t index,
                                    int direction){
    if(direction!=1 && direction!=-1)return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        auto& a=rule.at(condition?"conditions":"actions").get<picojson::array>();
        const auto dest=static_cast<std::ptrdiff_t>(index)+direction;
        if(index>=a.size() || dest<0 || dest>=static_cast<std::ptrdiff_t>(a.size()))
            throw std::invalid_argument("Cannot move node outside list");
        std::swap(a[index],a[static_cast<std::size_t>(dest)]);
    },"TRIGGER NODE MOVED");
}
bool EditorSession::setTriggerNodeField(RTSProject& project,bool condition,std::size_t index,
    const std::string& key,const picojson::value& value){
    if(key=="type")return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        auto& a=rule.at(condition?"conditions":"actions").get<picojson::array>();
        if(index>=a.size())throw std::invalid_argument("Node index out of range");
        auto& fields=a[index].get<picojson::object>();
        if(!fields.count(key))throw std::invalid_argument("Unknown node field");
        fields[key]=value;
    },"TRIGGER FIELD UPDATED");
}
bool EditorSession::setTriggerNodeOperand(RTSProject& project,
                                          bool condition,std::size_t index,
                                          bool useExpression,
                                          const std::string& variableId){
    return mutateTrigger(project,[&](picojson::object& rule){
        const auto bucket=condition?"conditions":"actions";
        auto& nodes=rule.at(bucket).get<picojson::array>();
        if(index>=nodes.size() || !nodes[index].is<picojson::object>())
            throw std::invalid_argument("Operand node index out of range");
        auto& node=nodes[index].get<picojson::object>();
        const auto type=node.at("type").get<std::string>();
        if(type!="VariableCompare" && type!="SetVariable" &&
           type!="AddVariable" && type!="GrantMinerals")
            throw std::invalid_argument("Node cannot accept a numerical expression");
        if(condition && type!="VariableCompare")
            throw std::invalid_argument("Invalid condition expression");
        const char* literal=(type=="GrantMinerals")?"amount":"value";
        const char* expression=(type=="GrantMinerals")?"amount_expr":"value_expr";
        if(useExpression){
            auto it=project.extensions.find("trigger_variables");
            if(it==project.extensions.end() || !it->second.is<picojson::object>() ||
               it->second.get<picojson::object>().empty())
                throw std::invalid_argument("Create a map variable before using an expression");
            const auto& available=it->second.get<picojson::object>();
            const std::string chosen=variableId.empty()?available.begin()->first:variableId;
            if(!available.count(chosen))
                throw std::invalid_argument("Expression references a missing variable");
            node.erase(literal);
            node[expression]=RTSScriptExpression::variable(chosen);
        }else{
            node.erase(expression);
            node[literal]=picojson::value(1.0);
        }
    },"SCRIPT OPERAND UPDATED");
}
bool EditorSession::addTriggerTreeNode(RTSProject& project,
                                       const TriggerTreePath& parent,
                                       const std::string& list,
                                       const std::string& type){
    const bool condition=list=="conditions";
    try{
        const auto node=TriggerTree::newNode(project,type,condition);
        return mutateTrigger(project,[&](picojson::object& rule){
            auto& nodes=TriggerTree::listAt(rule,parent,list);
            nodes.emplace_back(node);
        },condition?"NESTED CONDITION CREATED":"NESTED ACTION CREATED");
    }catch(const std::exception& e){
        message=std::string("TRIGGER REJECTED: ")+e.what();
        return false;
    }
}
bool EditorSession::removeTriggerTreeNode(RTSProject& project,
                                          const TriggerTreePath& path){
    if(path.empty())return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        TriggerTreePath parent(path.begin(),path.end()-1);
        auto& nodes=TriggerTree::listAt(rule,parent,path.back().list);
        if(path.back().index>=nodes.size())
            throw std::invalid_argument("Tree remove index out of range");
        nodes.erase(nodes.begin()+static_cast<std::ptrdiff_t>(path.back().index));
    },"NESTED TRIGGER NODE REMOVED");
}
bool EditorSession::duplicateTriggerTreeNode(RTSProject& project,
                                             const TriggerTreePath& path){
    if(path.empty())return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        TriggerTreePath parent(path.begin(),path.end()-1);
        auto& nodes=TriggerTree::listAt(rule,parent,path.back().list);
        const std::size_t index=path.back().index;
        if(index>=nodes.size())
            throw std::invalid_argument("Duplicate node index out of range");
        const picojson::value copy=nodes[index];
        nodes.insert(nodes.begin()+static_cast<std::ptrdiff_t>(index+1),copy);
    },"TRIGGER NODE DUPLICATED");
}
bool EditorSession::moveTriggerTreeNode(RTSProject& project,
                                        const TriggerTreePath& path,int direction){
    if(path.empty() || (direction!=-1 && direction!=1))return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        TriggerTreePath parent(path.begin(),path.end()-1);
        auto& nodes=TriggerTree::listAt(rule,parent,path.back().list);
        const auto to=static_cast<std::ptrdiff_t>(path.back().index)+direction;
        if(path.back().index>=nodes.size() || to<0 ||
           to>=static_cast<std::ptrdiff_t>(nodes.size()))
            throw std::invalid_argument("Cannot reorder outside action list");
        std::swap(nodes[path.back().index],nodes[static_cast<std::size_t>(to)]);
    },"TRIGGER NODE REORDERED");
}
bool EditorSession::setTriggerTreeField(RTSProject& project,
                                        const TriggerTreePath& path,
                                        const std::string& field,
                                        const picojson::value& value){
    if(path.empty() || field=="type" || field=="op" || field=="id" ||
       field=="conditions" || field=="actions" || field=="then" ||
       field=="else" || field=="value_expr" || field=="amount_expr" ||
       field=="times_expr")return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        auto& node=TriggerTree::objectAt(rule,path);
        auto existing=node.find(field);
        if(existing==node.end() ||
           (!existing->second.is<double>() && !existing->second.is<bool>() &&
            !existing->second.is<std::string>()))
            throw std::invalid_argument("Only existing scalar properties may be edited");
        node[field]=value;
    },"NESTED TRIGGER PROPERTY UPDATED");
}
bool EditorSession::setTriggerTreeOperand(RTSProject& project,
                                          const TriggerTreePath& path,
                                          bool expression,
                                          const std::string& variableId){
    if(path.empty())return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        auto& node=TriggerTree::objectAt(rule,path);
        const std::string& type=TriggerTree::typeOf(node);
        const bool repeat=type=="Repeat";
        if(!repeat && type!="VariableCompare" && type!="SetVariable" &&
           type!="AddVariable" && type!="GrantMinerals")
            throw std::invalid_argument("Unsupported expression property");
        const char* scalar=repeat?"times":type=="GrantMinerals"?"amount":"value";
        const char* expr=repeat?"times_expr":type=="GrantMinerals"?"amount_expr":"value_expr";
        if(expression){
            picojson::value tree=RTSScriptExpression::number(1);
            if(!variableId.empty()){
                const auto globals=parseTriggerVariables(project);
                if(!globals.count(variableId))
                    throw std::invalid_argument("Expression references missing map variable");
                tree=RTSScriptExpression::variable(variableId);
            }
            node.erase(scalar);
            node[expr]=std::move(tree);
        }else{
            node.erase(expr);
            node[scalar]=picojson::value(1.0);
        }
    },"TRIGGER EXPRESSION MODE UPDATED");
}
bool EditorSession::setTriggerExpressionNode(RTSProject& project,
                                             const TriggerTreePath& path,
                                             const std::string& field,
                                             const std::vector<std::string>& expressionPath,
                                             const picojson::value& expression){
    if(path.empty() || expressionPath.size()>8)return false;
    return mutateTrigger(project,[&](picojson::object& rule){
        auto& node=TriggerTree::objectAt(rule,path);
        auto& destination=TriggerTree::expressionAt(node,field,expressionPath);
        destination=expression;
    },"EXPRESSION TREE UPDATED");
}
bool EditorSession::beginTriggerEdit(const RTSProject& project) {
    if(textEditing || triggerCount(project)==0)return false;
    editText=triggerJSON(project,selectedTrigger%triggerCount(project));
    textEditing=true;newFieldEditing=false;
    message="EDIT JSON / CTRL+V PASTE / ENTER APPLY / ESC CANCEL";
    return true;
}
bool EditorSession::finishTriggerEdit(RTSProject& project) {
    if(!textEditing || triggerCount(project)==0)return false;
    picojson::value parsed;
    const std::string error=picojson::parse(parsed,editText);
    if(!error.empty() || !parsed.is<picojson::object>()){
        message="TRIGGER MUST BE A JSON OBJECT";
        return false;
    }
    const std::string id=triggerId(parsed);
    if(id.empty() || id=="<UNNAMED RULE>" || id=="<BAD RULE>"){
        message="TRIGGER REQUIRES STRING ID";
        return false;
    }
    const auto& rules=triggerArray(project);
    for(std::size_t i=0;i<rules.size();++i)
        if(i!=selectedTrigger%rules.size() && triggerId(rules[i])==id) {
            message="DUPLICATE TRIGGER ID";return false;
        }
    RTSProject candidate=project;
    try {
        auto& writable=mutableTriggers(candidate);
        writable[selectedTrigger%writable.size()]=std::move(parsed);
        candidate.validate();
        (void)parseTriggerRules(candidate);
    }catch(const std::exception& e) {
        message=std::string("INVALID TRIGGER JSON: ")+e.what();
        return false;
    }
    remember(project);
    project=std::move(candidate);
    textEditing=false;editText.clear();
    modified=true;message="TRIGGER JSON UPDATED";
    return true;
}
