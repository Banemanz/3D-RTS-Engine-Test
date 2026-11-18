#include "EditorSession.hpp"
#include "DataCatalog.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Behavior creator regression requires enabled assertions in Release"
#endif
#include <cmath>
#include <iostream>

static bool invalid(const RTSProject& project){
    try{project.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
int main(){
    RTSProject project=RTSProject::defaults();
    EditorSession ed;
    ed.panel=EditorSession::Panel::Data;
    assert(ed.createCatalogObject(project,"Behavior","Fortify"));
    const std::string guard=ed.selected(project)->id;
    assert(ed.selected(project)->parent.empty());
    const auto original=compileBehavior(project.catalog,guard);
    assert(original.maxStacks==3 && original.armorBonus==2.f);
    BehaviorDefinition draft=original;
    draft.durationTicks=240;
    draft.maxStacks=2;
    draft.armorBonus=3.25f;
    draft.damageBonus=4.f;
    draft.speedMultiplier=.8f;
    draft.dispellable=false;
    assert(ed.setBehaviorDefinition(project,draft));
    auto updated=compileBehavior(project.catalog,guard);
    assert(updated.durationTicks==240 && updated.maxStacks==2);
    assert(std::fabs(updated.armorBonus-3.25f)<.001f &&
           !updated.dispellable);
    assert(ed.undo(project));
    updated=compileBehavior(project.catalog,guard);
    assert(updated.durationTicks==original.durationTicks &&
           updated.dispellable==original.dispellable);
    assert(ed.redo(project));
    updated=compileBehavior(project.catalog,guard);
    assert(updated.durationTicks==240 && !updated.dispellable);
    const auto beforeInvalid=project.toJson().serialize();
    draft.speedMultiplier=0.f; // invalid; grouped transaction must roll back.
    assert(!ed.setBehaviorDefinition(project,draft));
    assert(project.toJson().serialize()==beforeInvalid);
    draft.speedMultiplier=.8f;
    draft.maxStacks=9;
    assert(!ed.setBehaviorDefinition(project,draft));
    assert(project.toJson().serialize()==beforeInvalid);

    assert(ed.createCatalogObject(project,"Behavior","Hinder"));
    const std::string hinder=ed.selected(project)->id;
    assert(compileBehavior(project.catalog,hinder).speedMultiplier<1.f);
    assert(ed.createCatalogObject(project,"Behavior","Sharpen"));
    const std::string focus=ed.selected(project)->id;
    assert(compileBehavior(project.catalog,focus).damageBonus>0.f);
    assert(ed.createCatalogObject(project,"Effect","ApplyBehavior"));
    const std::string apply=ed.selected(project)->id;
    assert(compileEffect(project.catalog,apply).size()==1);
    assert(compileEffect(project.catalog,apply)[0].kind==
           EffectOperationKind::ApplyBehavior);
    assert(ed.setCatalogField(project,"behavior",picojson::value(hinder)));
    assert(compileEffect(project.catalog,apply)[0].behaviorId==hinder);
    assert(ed.convertEffectKind(project,"RemoveBehavior"));
    assert(compileEffect(project.catalog,apply)[0].kind==
           EffectOperationKind::RemoveBehavior);
    assert(ed.undo(project));
    assert(compileEffect(project.catalog,apply)[0].kind==
           EffectOperationKind::ApplyBehavior);
    assert(ed.redo(project));
    assert(compileEffect(project.catalog,apply)[0].kind==
           EffectOperationKind::RemoveBehavior);
    assert(ed.createCatalogObject(project,"Effect","ApplyBehavior"));
    const std::string second=ed.selected(project)->id;
    assert(ed.setCatalogField(project,"behavior",picojson::value(focus)));
    assert(ed.createCatalogObject(project,"Effect","Sequence"));
    const std::string sequence=ed.selected(project)->id;
    assert(ed.setCatalogField(project,"children",picojson::value(
        picojson::array{picojson::value(apply),picojson::value(second)})));
    auto compiled=compileEffect(project.catalog,sequence);
    assert(compiled.size()==2);
    assert(compiled[0].kind==EffectOperationKind::RemoveBehavior);
    assert(compiled[1].kind==EffectOperationKind::ApplyBehavior);
    assert(ed.setCatalogField(project,"children",picojson::value(
        picojson::array{picojson::value(second),picojson::value(apply)})));
    compiled=compileEffect(project.catalog,sequence);
    assert(compiled[0].kind==EffectOperationKind::ApplyBehavior);
    assert(compiled[1].kind==EffectOperationKind::RemoveBehavior);
    assert(ed.undo(project));
    compiled=compileEffect(project.catalog,sequence);
    assert(compiled[0].kind==EffectOperationKind::RemoveBehavior);
    assert(ed.redo(project));
    compiled=compileEffect(project.catalog,sequence);
    assert(compiled[0].kind==EffectOperationKind::ApplyBehavior);

    std::string error;
    assert(!project.removeCatalogObject(hinder,&error));
    assert(error.find("behavior")!=std::string::npos);
    assert(ed.selectCatalogId(project,hinder));
    assert(ed.beginCatalogRename(project));
    ed.editText="new_hinder_modifier";
    assert(ed.finishCatalogRename(project));
    const auto migrated=compileEffect(project.catalog,apply);
    assert(migrated.size()==1 && migrated[0].behaviorId=="new_hinder_modifier");
    assert(ed.undo(project));
    assert(compileEffect(project.catalog,apply)[0].behaviorId==hinder);
    assert(ed.redo(project));
    assert(compileEffect(project.catalog,apply)[0].behaviorId=="new_hinder_modifier");
    auto invalidProject=project;
    assert(invalidProject.catalog.setField("new_hinder_modifier","duration_ticks",
                                           picojson::value(36001.0)));
    assert(invalid(invalidProject));
    invalidProject=project;
    assert(invalidProject.catalog.setField(apply,"behavior",
                                           picojson::value("invalid")));
    assert(invalid(invalidProject));
    invalidProject=project;
    assert(invalidProject.catalog.setField(sequence,"children",
        picojson::value(picojson::array{picojson::value(sequence)})));
    assert(invalid(invalidProject));
    const RTSProject roundTrip=RTSProject::fromJson(project.toJson());
    assert(compileEffect(roundTrip.catalog,sequence).size()==2);
    assert(compileBehavior(roundTrip.catalog,"new_hinder_modifier").maxStacks==1);
    std::cout<<"F7 Behavior creator, grouped edits, effect graph reorder, undo/redo and typed refs passed\n";
}
