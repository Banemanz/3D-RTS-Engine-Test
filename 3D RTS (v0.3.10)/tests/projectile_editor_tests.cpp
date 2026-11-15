#include "EditorSession.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Projectile F7 editor regression requires active asserts"
#endif
#include <iostream>
#include <stdexcept>

namespace {
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(){
    RTSProject p=RTSProject::defaults();
    EditorSession editor;editor.panel=EditorSession::Panel::Data;
    assert(editor.createCatalogObject(p,"Projectile","Arc"));
    const auto arcId=editor.selected(p)->id;
    const auto starter=compileProjectile(p.catalog,arcId);
    assert(starter.arcHeight==3.f && !starter.collideUnits);
    auto draft=starter;
    draft.speed=18.f;draft.arcHeight=7.f;draft.visualScale=.8f;
    draft.collisionRadius=.7f;draft.collideUnits=true;
    draft.tint=Vec3(.3f,.5f,.9f);
    assert(editor.setProjectileDefinition(p,draft));
    auto compiled=compileProjectile(p.catalog,arcId);
    assert(compiled.speed==18.f && compiled.arcHeight==7.f &&
           compiled.collideUnits && compiled.tint.z==.9f);
    const auto valid=p.toJson().serialize();
    draft.speed=0.f;
    assert(!editor.setProjectileDefinition(p,draft));
    assert(p.toJson().serialize()==valid);
    draft.speed=18.f;draft.tint.x=2.f;
    assert(!editor.setProjectileDefinition(p,draft));
    assert(p.toJson().serialize()==valid);
    assert(editor.undo(p));
    compiled=compileProjectile(p.catalog,arcId);
    assert(compiled.speed==starter.speed && !compiled.collideUnits);
    assert(editor.redo(p));
    assert(compileProjectile(p.catalog,arcId).collideUnits);

    assert(editor.createCatalogObject(p,"Projectile","Bolt"));
    const auto boltId=editor.selected(p)->id;
    assert(compileProjectile(p.catalog,boltId).collideUnits);
    assert(editor.createCatalogObject(p,"Ability","Ground"));
    const auto groundId=editor.selected(p)->id;
    assert(editor.setCatalogField(p,"projectile",picojson::value(arcId)));
    auto ability=compileAbility(p.catalog,groundId);
    assert(ability.projectileId==arcId);
    assert(editor.undo(p));
    assert(compileAbility(p.catalog,groundId).projectileId.empty());
    assert(editor.redo(p));
    assert(compileAbility(p.catalog,groundId).projectileId==arcId);
    assert(editor.setCatalogField(p,"projectile",picojson::value(boltId)));
    assert(compileAbility(p.catalog,groundId).projectileId==boltId);
    std::string error;
    assert(!p.removeCatalogObject(boltId,&error));
    assert(error.find(".projectile")!=std::string::npos);
    assert(p.renameCatalogObject(boltId,"bolt_renamed"));
    assert(compileAbility(p.catalog,groundId).projectileId=="bolt_renamed");
    assert(p.catalogReferences("bolt_renamed").size()==1);
    const auto copy=RTSProject::fromJson(p.toJson());
    assert(compileAbility(copy.catalog,groundId).projectileId=="bolt_renamed");
    assert(compileProjectile(copy.catalog,"bolt_renamed").collideUnits);

    // Leaving Ground clears the typed reference as one undoable
    // operation instead of producing a half-invalid Ability.
    assert(editor.selectCatalogId(p,groundId));
    assert(editor.setAbilityTargetPolicy(p,"Self"));
    ability=compileAbility(p.catalog,groundId);
    assert(ability.target==AbilityTarget::Self && ability.projectileId.empty());
    assert(editor.undo(p));
    assert(compileAbility(p.catalog,groundId).projectileId=="bolt_renamed");
    assert(editor.redo(p));
    assert(compileAbility(p.catalog,groundId).projectileId.empty());
    assert(editor.setAbilityTargetPolicy(p,"Ground"));
    assert(compileAbility(p.catalog,groundId).projectileId.empty());
    assert(editor.setCatalogField(p,"projectile",
                                  picojson::value(std::string("bolt_renamed"))));
    assert(compileAbility(p.catalog,groundId).projectileId=="bolt_renamed");

    // Inherited Projectile reference must be *overridden*, not erased.
    // Changing the derived target cannot modify the original parent.
    assert(p.catalog.cloneDerived(groundId,"derived_ground_skill"));
    p.validate();
    assert(editor.selectCatalogId(p,"derived_ground_skill"));
    assert(editor.setAbilityTargetPolicy(p,"Self"));
    assert(compileAbility(p.catalog,"derived_ground_skill").target==
           AbilityTarget::Self);
    assert(compileAbility(p.catalog,"derived_ground_skill").projectileId.empty());
    assert(compileAbility(p.catalog,groundId).projectileId=="bolt_renamed");
    assert(editor.undo(p));
    assert(compileAbility(p.catalog,"derived_ground_skill").projectileId==
           "bolt_renamed");
    assert(editor.redo(p));
    assert(compileAbility(p.catalog,"derived_ground_skill").projectileId.empty());

    auto broken=p;
    assert(broken.catalog.setField("bolt_renamed","tint",
        picojson::value(picojson::array{
            picojson::value(.3),picojson::value(-1.),picojson::value(.5)})));
    assert(invalid(broken));
    broken=p;
    assert(broken.catalog.setField(groundId,"projectile",
                                   picojson::value("not_a_projectile")));
    assert(invalid(broken));
    std::cout<<"F7 projectile creation/validation, grouped editing, links, rename, undo/redo and target conversion passed\n";
}
