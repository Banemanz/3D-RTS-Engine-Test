#include "EditorSession.hpp"
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
    const RTSProject before=project;
    auto& writable=mutableTriggers(project);
    writable[selectedTrigger%writable.size()]=std::move(parsed);
    remember(before);
    textEditing=false;editText.clear();
    modified=true;message="TRIGGER JSON UPDATED";
    return true;
}
