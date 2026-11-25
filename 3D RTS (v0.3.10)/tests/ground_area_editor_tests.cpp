#include "EditorSession.hpp"
#include "DataCatalog.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Ground area editor tests require assertions in optimized builds"
#endif
#include <iostream>

namespace {
bool invalid(const RTSProject& project){
    try{project.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(){
    RTSProject project=RTSProject::defaults();
    EditorSession editor;editor.panel=EditorSession::Panel::Data;
    assert(editor.createCatalogObject(project,"Ability","Ground"));
    const std::string id=editor.selected(project)->id;
    assert(editor.selected(project)->type=="Ability");
    auto ground=compileAbility(project.catalog,id);
    assert(ground.target==AbilityTarget::Ground);
    assert(ground.areaRadius==4.f);
    assert(ground.impactDelayTicks==60);
    assert(ground.areaTeam==AbilityAreaTeam::Enemy);
    assert(ground.castRange==12.f);
    assert(editor.setGroundAbilitySettings(project,8.5f,"Any",150));
    ground=compileAbility(project.catalog,id);
    assert(ground.areaRadius==8.5f);
    assert(ground.impactDelayTicks==150);
    assert(ground.areaTeam==AbilityAreaTeam::Any);
    const std::string validState=project.toJson().serialize();
    assert(!editor.setGroundAbilitySettings(project,0.f,"Any",150));
    assert(!editor.setGroundAbilitySettings(project,8.5f,"None",150));
    assert(!editor.setGroundAbilitySettings(project,8.5f,"Any",-1));
    assert(!editor.setGroundAbilitySettings(project,8.5f,"Any",301));
    assert(project.toJson().serialize()==validState);
    assert(editor.undo(project));
    ground=compileAbility(project.catalog,id);
    assert(ground.areaRadius==4.f && ground.impactDelayTicks==60);
    assert(editor.redo(project));
    ground=compileAbility(project.catalog,id);
    assert(ground.areaRadius==8.5f && ground.impactDelayTicks==150);

    // Transition to unit and self targeting without invalid intermediate
    // states; a Self ability may temporarily retain unused radius fields.
    assert(editor.setAbilityTargetPolicy(project,"Self"));
    assert(compileAbility(project.catalog,id).target==AbilityTarget::Self);
    assert(editor.setAbilityTargetPolicy(project,"Ground"));
    ground=compileAbility(project.catalog,id);
    assert(ground.target==AbilityTarget::Ground);
    assert(ground.areaRadius==4.f && ground.impactDelayTicks==60);
    assert(editor.undo(project));
    assert(compileAbility(project.catalog,id).target==AbilityTarget::Self);
    assert(editor.redo(project));
    assert(compileAbility(project.catalog,id).target==AbilityTarget::Ground);
    assert(editor.setAbilityTargetPolicy(project,"Ally"));
    assert(compileAbility(project.catalog,id).target==AbilityTarget::Ally);
    assert(editor.setAbilityTargetPolicy(project,"Ground"));
    assert(compileAbility(project.catalog,id).target==AbilityTarget::Ground);

    // Convert a zero-range Self Ability to Ground with the necessary valid
    // cast_range, area_radius, delay and team in one undoable transaction.
    assert(editor.createCatalogObject(project,"Ability","Self"));
    const std::string selfId=editor.selected(project)->id;
    assert(compileAbility(project.catalog,selfId).castRange==0.f);
    assert(editor.setAbilityTargetPolicy(project,"Ground"));
    const auto converted=compileAbility(project.catalog,selfId);
    assert(converted.target==AbilityTarget::Ground);
    assert(converted.castRange==12.f);
    assert(converted.areaRadius==4.f);
    assert(converted.areaTeam==AbilityAreaTeam::Enemy);
    assert(editor.undo(project));
    assert(compileAbility(project.catalog,selfId).target==AbilityTarget::Self);
    assert(editor.redo(project));
    assert(compileAbility(project.catalog,selfId).castRange==12.f);
    assert(editor.setGroundAbilitySettings(project,5.f,"Ally",0));
    assert(compileAbility(project.catalog,selfId).areaTeam==AbilityAreaTeam::Ally);
    assert(compileAbility(project.catalog,selfId).impactDelayTicks==0);

    // The authoring workflow still validates links and inherited Unit loadouts.
    assert(editor.selectCatalogId(project,"ranger"));
    assert(editor.setCatalogField(project,"abilities",picojson::value(
        picojson::array{picojson::value(id),picojson::value(selfId)})));
    assert(unitAbilityIds(project.catalog,"ranger").size()==2);
    std::string why;
    assert(!project.removeCatalogObject(id,&why));
    assert(why.find("abilities")!=std::string::npos);
    assert(project.renameCatalogObject(id,"renamed_ground_ability"));
    assert(unitAbilityIds(project.catalog,"ranger")[0]=="renamed_ground_ability");
    const auto roundTrip=RTSProject::fromJson(project.toJson());
    assert(compileAbility(roundTrip.catalog,"renamed_ground_ability").areaRadius==4.f);
    assert(compileAbility(roundTrip.catalog,selfId).areaTeam==AbilityAreaTeam::Ally);
    auto broken=project;
    assert(broken.catalog.setField(selfId,"impact_delay_ticks",picojson::value(1.25)));
    assert(invalid(broken));
    broken=project;
    assert(broken.catalog.setField(selfId,"area_radius",picojson::value(25.0)));
    assert(invalid(broken));
    broken=project;
    assert(broken.catalog.setField(selfId,"area_team",picojson::value("Environment")));
    assert(invalid(broken));
    std::cout<<"F7 Ground Ability presets, grouped area editor, target conversion, undo and refs passed\n";
}
