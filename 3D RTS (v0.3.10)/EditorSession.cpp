#include "EditorSession.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

std::string EditorSession::panelName()const {
    switch(panel) {
        case Panel::Play:return "PLAY";
        case Panel::Terrain:return "TERRAIN EDITOR";
        case Panel::Data:return "DATA EDITOR";
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
    painting_=true;
    strokeActionRecorded_=false;
    project.terrain.beginStroke();
}
void EditorSession::finishStroke(RTSProject& project) {
    if(!painting_)return;
    painting_=false;
    strokeActionRecorded_=false;
    project.terrain.endStroke();
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
            history_.push_back(false);
            if(history_.size()>maxHistory_)history_.pop_front();
        }
    }
    return changed;
}
void EditorSession::rememberPlacement(const RTSProject& project) {
    placementUndo_.push_back(project.placements);
    while(placementUndo_.size()>maxHistory_)placementUndo_.pop_front();
    placementRedo_.clear();
    history_.push_back(true);
    while(history_.size()>maxHistory_)history_.pop_front();
}
bool EditorSession::undo(RTSProject& project) {
    finishStroke(project);
    if(history_.empty())return project.terrain.undo();
    bool placement=history_.back();history_.pop_back();
    if(!placement)return project.terrain.undo();
    if(placementUndo_.empty())return false;
    placementRedo_.push_back(project.placements);
    project.placements=std::move(placementUndo_.back());
    placementUndo_.pop_back();
    modified=true;
    return true;
}
bool EditorSession::redo(RTSProject& project) {
    finishStroke(project);
    if(!placementRedo_.empty()) {
        placementUndo_.push_back(project.placements);
        project.placements=std::move(placementRedo_.back());
        placementRedo_.pop_back();
        history_.push_back(true);
        modified=true;return true;
    }
    return project.terrain.redo();
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
void EditorSession::cycleObject(const RTSProject& project,int delta) {
    if(textEditing || project.catalog.entries().empty())return;
    int count=int(project.catalog.entries().size());
    selectedObject=static_cast<std::size_t>((int(selectedObject%count)+delta%count+count)%count);
    selectedField=0;
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
    textEditing=true;newFieldEditing=false;
    message="ENTER TO APPLY FIELD / ESC TO CANCEL";
    return true;
}
void EditorSession::beginNewField(){
    if(textEditing)return;
    textEditing=true;newFieldEditing=true;pendingNewField_.clear();editText.clear();
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
    textEditing=false;newFieldEditing=false;editText.clear();pendingNewField_.clear();
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
    auto* selectedEntry=project.catalog.entries().empty()?nullptr:
        &project.catalog.entries()[selectedObject%project.catalog.entries().size()];
    if(!selectedEntry)return false;
    std::string key=pendingNewField_.empty()?selectedFieldName(project):pendingNewField_;
    if(key.empty())return false;
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
    auto old=mutableEntry->fields.at(key);
    mutableEntry->fields.erase(key);
    try{project.validate();}
    catch(...){mutableEntry->fields[key]=old;return false;}
    modified=true;message="OVERRIDE REMOVED";return true;
}
bool EditorSession::duplicateCatalogObject(RTSProject& project) {
    const auto* e=selected(project);if(!e)return false;
    const std::string base=e->id+"_copy";
    std::string next=base;
    int suffix=2;
    while(project.catalog.find(next))next=base+std::to_string(suffix++);
    if(!project.catalog.cloneDerived(e->id,next))return false;
    selectedObject=project.catalog.entries().size()-1;
    selectedField=0;modified=true;message="DERIVED "+next;return true;
}
