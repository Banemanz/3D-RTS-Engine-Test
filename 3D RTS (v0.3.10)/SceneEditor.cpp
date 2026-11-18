#include "EditorSession.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

const ProjectPlacement* EditorSession::selectedPlacement(const RTSProject& project)const {
    for(const auto& p:project.placements)if(p.id==selectedPlacementId)return &p;
    return nullptr;
}
bool EditorSession::selectNearestPlacement(const RTSProject& project,
                                           const Vec3& world,float radius) {
    const ProjectPlacement* nearest=nullptr;
    float best=std::max(0.f,radius);
    for(const auto& p:project.placements) {
        const float d=(p.position-world).lengthXZ();
        if(d<=best){best=d;nearest=&p;}
    }
    selectedPlacementId=nearest?nearest->id:"";
    message=nearest?"SELECTED "+nearest->id:"NO PLACEMENT NEAR CURSOR";
    return nearest!=nullptr;
}
bool EditorSession::cycleSceneSelection(const RTSProject& project,int delta) {
    const auto& a=project.placements;
    if(a.empty()){selectedPlacementId.clear();return false;}
    int index=0;
    for(std::size_t i=0;i<a.size();++i)
        if(a[i].id==selectedPlacementId){index=int(i);break;}
    index=(index+delta%int(a.size())+int(a.size()))%int(a.size());
    selectedPlacementId=a[static_cast<std::size_t>(index)].id;
    message="SELECTED "+selectedPlacementId;
    return true;
}
bool EditorSession::moveSelectedPlacement(RTSProject& project,const Vec3& world) {
    if(!project.terrain.insideWorld(world.x,world.z))return false;
    auto i=std::find_if(project.placements.begin(),project.placements.end(),
                       [&](const ProjectPlacement& p){return p.id==selectedPlacementId;});
    if(i==project.placements.end())return false;
    const Vec3 target=project.terrain.clampToMap(world);
    if((target-i->position).length()<.0001f)return false;
    const RTSProject before=project;
    i->position=target;
    try{project.validate();}
    catch(...){project=before;throw;}
    remember(before);
    modified=true;message="MOVED "+selectedPlacementId;
    return true;
}
bool EditorSession::rotateSelectedPlacement(RTSProject& project,float degrees) {
    if(!std::isfinite(degrees) || std::abs(degrees)>360)return false;
    auto i=std::find_if(project.placements.begin(),project.placements.end(),
                       [&](const ProjectPlacement& p){return p.id==selectedPlacementId;});
    if(i==project.placements.end())return false;
    const RTSProject before=project;
    i->rotation.y=std::fmod(i->rotation.y+degrees+360.f,360.f);
    remember(before);
    modified=true;message="ROTATED "+selectedPlacementId;
    return true;
}
bool EditorSession::scaleSelectedPlacement(RTSProject& project,float factor) {
    if(!std::isfinite(factor) || factor<=0 || factor==1)return false;
    auto i=std::find_if(project.placements.begin(),project.placements.end(),
                       [&](const ProjectPlacement& p){return p.id==selectedPlacementId;});
    if(i==project.placements.end())return false;
    const RTSProject before=project;
    i->scale=Vec3(std::clamp(i->scale.x*factor,.1f,20.f),
                  std::clamp(i->scale.y*factor,.1f,20.f),
                  std::clamp(i->scale.z*factor,.1f,20.f));
    remember(before);
    modified=true;message="SCALED "+selectedPlacementId;
    return true;
}
bool EditorSession::toggleSelectedOwner(RTSProject& project) {
    auto i=std::find_if(project.placements.begin(),project.placements.end(),
                       [&](const ProjectPlacement& p){return p.id==selectedPlacementId;});
    if(i==project.placements.end() || i->kind=="Mineral")return false;
    const RTSProject before=project;
    i->owner=i->owner==0?1:0;
    remember(before);
    modified=true;message="CHANGED OWNER OF "+selectedPlacementId;
    return true;
}
bool EditorSession::adjustSelectedRegionRadius(RTSProject& project,float delta) {
    auto i=std::find_if(project.placements.begin(),project.placements.end(),
                       [&](const ProjectPlacement& p){return p.id==selectedPlacementId;});
    if(i==project.placements.end() || i->kind!="Region")return false;
    float radius=5.0f;
    auto old=i->overrides.find("radius");
    if(old!=i->overrides.end() && old->second.is<double>())
        radius=float(old->second.get<double>());
    const float next=std::clamp(radius+delta,.5f,64.f);
    if(next==radius)return false;
    const RTSProject before=project;
    i->overrides["radius"]=picojson::value(double(next));
    remember(before);
    modified=true;message="REGION RADIUS "+std::to_string(next);
    return true;
}
bool EditorSession::duplicateSelectedPlacement(RTSProject& project) {
    const auto* selected=selectedPlacement(project);
    if(!selected || project.placements.size()>=20000)return false;
    ProjectPlacement copy=*selected;
    const std::string base=copy.id+"_copy";
    if(base.size()>124)return false;
    copy.id=base;
    int n=2;
    while(std::any_of(project.placements.begin(),project.placements.end(),
                      [&](const ProjectPlacement& p){return p.id==copy.id;}))
        copy.id=base+std::to_string(n++);
    copy.name+=" Copy";
    copy.position=project.terrain.clampToMap(copy.position+Vec3(2,0,2));
    const RTSProject before=project;
    project.placements.push_back(copy);
    try{project.validate();}
    catch(...){project=before;throw;}
    remember(before);
    selectedPlacementId=copy.id;
    modified=true;message="DUPLICATED "+copy.id;
    return true;
}
bool EditorSession::deleteSelectedPlacement(RTSProject& project) {
    auto i=std::find_if(project.placements.begin(),project.placements.end(),
                       [&](const ProjectPlacement& p){return p.id==selectedPlacementId;});
    if(i==project.placements.end())return false;
    const std::size_t index=static_cast<std::size_t>(i-project.placements.begin());
    const RTSProject before=project;
    project.placements.erase(i);
    selectedPlacementId=project.placements.empty()?""
        :project.placements[std::min(index,project.placements.size()-1)].id;
    remember(before);
    modified=true;message="PLACEMENT REMOVED";
    return true;
}
bool EditorSession::placeSceneMarker(RTSProject& project,const Vec3& world,bool region) {
    if(!project.terrain.insideWorld(world.x,world.z) ||
       project.placements.size()>=20000)return false;
    ProjectPlacement p;
    p.kind=region?"Region":"Point";
    p.name=region?"Region":"Point";
    const std::string base=region?"region_":"point_";
    int n=1;
    do{p.id=base+std::to_string(n++);}
    while(std::any_of(project.placements.begin(),project.placements.end(),
        [&](const ProjectPlacement& x){return x.id==p.id;}));
    p.position=project.terrain.clampToMap(world);
    p.owner=-1;
    if(region)p.overrides["radius"]=picojson::value(5.0);
    const RTSProject before=project;
    project.placements.push_back(p);
    remember(before);
    selectedPlacementId=p.id;modified=true;
    message=std::string("CREATED ")+p.kind+" "+p.id;
    return true;
}
bool EditorSession::beginSceneOverrideEdit(const RTSProject& project) {
    if(textEditing)return false;
    const auto* p=selectedPlacement(project);
    if(!p)return false;
    editText=picojson::value(p->overrides).serialize();
    newFieldEditing=false;textEditing=true;
    message="EDIT PLACEMENT OVERRIDES AS JSON; ENTER APPLIES";
    return true;
}
bool EditorSession::finishSceneOverrideEdit(RTSProject& project) {
    if(!textEditing)return false;
    picojson::value value;
    const auto error=picojson::parse(value,editText);
    if(!error.empty() || !value.is<picojson::object>()) {
        message="OVERRIDES MUST BE A VALID JSON OBJECT";
        return false;
    }
    auto i=std::find_if(project.placements.begin(),project.placements.end(),
                       [&](const ProjectPlacement& p){return p.id==selectedPlacementId;});
    if(i==project.placements.end())return false;
    const RTSProject before=project;
    i->overrides=value.get<picojson::object>();
    try{project.validate();}
    catch(const std::exception& e){
        project=before;
        message=std::string("INVALID INSTANCE OVERRIDES: ")+e.what();
        return false;
    }
    remember(before);
    textEditing=false;editText.clear();
    modified=true;message="INSTANCE OVERRIDES UPDATED";
    return true;
}
