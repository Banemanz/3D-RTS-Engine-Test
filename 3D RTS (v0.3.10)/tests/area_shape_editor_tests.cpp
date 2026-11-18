#include "EditorSession.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Shape F7 editor test needs Release assertions"
#endif
#include <iostream>
int main(){
    RTSProject p=RTSProject::defaults();
    EditorSession e;e.panel=EditorSession::Panel::Data;
    assert(e.createCatalogObject(p,"Ability","Ground"));
    const std::string id=e.selected(p)->id;
    const auto initial=p.toJson().serialize();
    assert(compileAbility(p.catalog,id).areaShape==RTSAreaShape::Circle);
    assert(e.setGroundAbilitySettings(p,6.f,"Any",45,"Cone",75.f,1.75f));
    auto d=compileAbility(p.catalog,id);
    assert(d.areaShape==RTSAreaShape::Cone && d.areaAngle==75.f);
    const auto valid=p.toJson().serialize();
    assert(!e.setGroundAbilitySettings(p,6.f,"Any",45,"Ellipse",75.f,1.75f));
    assert(!e.setGroundAbilitySettings(p,6.f,"Any",45,"Cone",175.f,1.75f));
    assert(!e.setGroundAbilitySettings(p,6.f,"Any",45,"Line",75.f,.1f));
    assert(p.toJson().serialize()==valid);
    assert(e.undo(p) && p.toJson().serialize()==initial);
    assert(e.redo(p) && p.toJson().serialize()==valid);
    assert(compileAbility(RTSProject::fromJson(p.toJson()).catalog,id)
               .areaShape==RTSAreaShape::Cone);
    assert(p.catalog.cloneDerived(id,"child_area"));
    p.validate();
    assert(e.selectCatalogId(p,"child_area"));
    assert(e.setGroundAbilitySettings(p,7.f,"Enemy",0,"Line",60.f,2.5f));
    assert(compileAbility(p.catalog,"child_area").areaShape==RTSAreaShape::Line);
    assert(compileAbility(p.catalog,id).areaShape==RTSAreaShape::Cone);
    assert(e.undo(p));
    assert(compileAbility(p.catalog,"child_area").areaShape==RTSAreaShape::Cone);
    assert(e.redo(p));
    assert(compileAbility(p.catalog,"child_area").areaWidth==2.5f);
    assert(e.setAbilityTargetPolicy(p,"Self"));
    assert(compileAbility(p.catalog,"child_area").target==AbilityTarget::Self);
    assert(e.setAbilityTargetPolicy(p,"Ground"));
    assert(compileAbility(p.catalog,"child_area").areaShape==RTSAreaShape::Circle);
    std::cout<<"F7 shaped area authoring, validation, inheritance, undo/redo passed\n";
}
