#include "EditorSession.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Weapon Projectile editor tests require Release assertions"
#endif
#include <iostream>
int main(){
    RTSProject p=RTSProject::defaults();
    EditorSession e;e.panel=EditorSession::Panel::Data;
    assert(e.createCatalogObject(p,"Projectile","Bolt"));
    const std::string projectile=e.selected(p)->id;
    assert(e.createCatalogObject(p,"Weapon","Ranged"));
    const std::string weapon=e.selected(p)->id;
    WeaponDefinition definition=compileWeapon(p.catalog,weapon);
    assert(definition.projectileId.empty());
    definition.projectileId=projectile;
    assert(e.setWeaponDefinition(p,definition));
    assert(compileWeapon(p.catalog,weapon).projectileId==projectile);
    const std::string valid=p.toJson().serialize();
    definition.projectileId="missing_bolt";
    assert(!e.setWeaponDefinition(p,definition));
    assert(p.toJson().serialize()==valid);
    assert(e.undo(p));
    assert(compileWeapon(p.catalog,weapon).projectileId.empty());
    assert(e.redo(p));
    assert(compileWeapon(p.catalog,weapon).projectileId==projectile);
    assert(e.selectCatalogId(p,"ranger"));
    assert(e.setCatalogField(p,"weapon",picojson::value(weapon)));
    assert(unitFromCatalog(p.catalog,"ranger").projectileId==projectile);
    assert(p.catalog.cloneDerived("ranger","derived_launcher"));
    p.validate();
    assert(unitFromCatalog(p.catalog,"derived_launcher").projectileId==projectile);
    std::string why;
    assert(!p.removeCatalogObject(projectile,&why));
    assert(why.find(".projectile")!=std::string::npos);
    assert(p.renameCatalogObject(projectile,"renamed_bolt"));
    assert(compileWeapon(p.catalog,weapon).projectileId=="renamed_bolt");
    assert(unitFromCatalog(p.catalog,"derived_launcher").projectileId=="renamed_bolt");
    const auto copy=RTSProject::fromJson(p.toJson());
    assert(compileWeapon(copy.catalog,weapon).projectileId=="renamed_bolt");
    assert(e.selectCatalogId(p,weapon));
    definition=compileWeapon(p.catalog,weapon);
    definition.projectileId="";
    assert(e.setWeaponDefinition(p,definition));
    assert(unitFromCatalog(p.catalog,"ranger").projectileId.empty());
    assert(e.undo(p));
    assert(unitFromCatalog(p.catalog,"ranger").projectileId=="renamed_bolt");
    assert(e.redo(p));
    assert(unitFromCatalog(p.catalog,"ranger").projectileId.empty());
    std::cout<<"F7 Weapon/Projectile links, rename, inherited refs, undo/redo passed\n";
}
