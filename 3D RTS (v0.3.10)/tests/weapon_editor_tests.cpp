#include "EditorSession.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Weapon F7 editor regression requires active Release assertions"
#endif
#include <iostream>
int main(){
    RTSProject p=RTSProject::defaults();
    EditorSession editor;editor.panel=EditorSession::Panel::Data;
    assert(editor.createCatalogObject(p,"Weapon","Ranged"));
    const std::string weapon=editor.selected(p)->id;
    auto initial=compileWeapon(p.catalog,weapon);
    assert(initial.damage==8.f && initial.armorPiercing==1.f);
    WeaponDefinition draft=initial;
    draft.damage=18.f;draft.range=9.f;
    draft.cooldown=.65f;draft.armorPiercing=4.f;
    assert(editor.setWeaponDefinition(p,draft));
    auto applied=compileWeapon(p.catalog,weapon);
    assert(applied.damage==18.f && applied.armorPiercing==4.f);
    const auto valid=p.toJson().serialize();
    draft.cooldown=0.f;
    assert(!editor.setWeaponDefinition(p,draft));
    draft.cooldown=.65f;draft.armorPiercing=-1.f;
    assert(!editor.setWeaponDefinition(p,draft));
    assert(p.toJson().serialize()==valid);
    assert(editor.undo(p));
    assert(compileWeapon(p.catalog,weapon).damage==initial.damage);
    assert(editor.redo(p));
    assert(compileWeapon(p.catalog,weapon).damage==18.f);
    assert(editor.selectCatalogId(p,"ranger"));
    assert(editor.setCatalogField(p,"weapon",picojson::value(weapon)));
    assert(unitFromCatalog(p.catalog,"ranger").damage==18.f);
    assert(!editor.setCatalogField(p,"weapon",picojson::value("not_a_weapon")));
    assert(unitFromCatalog(p.catalog,"ranger").weaponId==weapon);
    assert(p.catalog.cloneDerived("ranger","piercer_unit"));
    p.validate();
    assert(unitFromCatalog(p.catalog,"piercer_unit").weaponId==weapon);
    std::string why;
    assert(!p.removeCatalogObject(weapon,&why));
    assert(why.find(".weapon")!=std::string::npos);
    assert(p.renameCatalogObject(weapon,"renamed_piercer"));
    assert(unitFromCatalog(p.catalog,"ranger").weaponId=="renamed_piercer");
    assert(unitFromCatalog(p.catalog,"piercer_unit").weaponId=="renamed_piercer");
    assert(p.catalogReferences("renamed_piercer").size()>=2);
    const auto copy=RTSProject::fromJson(p.toJson());
    assert(unitFromCatalog(copy.catalog,"piercer_unit").damage==18.f);
    assert(editor.selectCatalogId(p,"piercer_unit"));
    assert(editor.setCatalogField(p,"weapon",picojson::value("")));
    assert(unitFromCatalog(p.catalog,"piercer_unit").weaponId.empty());
    assert(unitFromCatalog(p.catalog,"ranger").weaponId=="renamed_piercer");
    assert(editor.undo(p));
    assert(unitFromCatalog(p.catalog,"piercer_unit").weaponId=="renamed_piercer");
    assert(editor.redo(p));
    assert(unitFromCatalog(p.catalog,"piercer_unit").weaponId.empty());
    std::cout<<"F7 Weapon objects, typed refs, inheritance, rollback, rename and undo/redo passed\n";
}
