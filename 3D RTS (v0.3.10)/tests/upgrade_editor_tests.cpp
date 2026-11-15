#include "EditorSession.hpp"
#include "RTSCatalogNavigator.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Upgrade editor test must retain assertions in Release"
#endif
#include <iostream>
#include <stdexcept>
int main(){
    RTSProject p=RTSProject::defaults();
    EditorSession ed;ed.panel=EditorSession::Panel::Data;
    assert(ed.createCatalogObject(p,"Upgrade","Damage"));
    const std::string id=ed.selected(p)->id;
    auto draft=compileUpgrade(p.catalog,id);
    assert(draft.maxLevel==3 && draft.damagePerLevel==2.f);
    draft.unitId="ranger";draft.maxLevel=4;
    draft.armorPerLevel=1.5f;draft.damagePerLevel=3.f;
    assert(ed.setUpgradeDefinition(p,draft));
    const auto good=p.toJson().serialize();
    auto malformed=draft;malformed.maxLevel=6;
    assert(!ed.setUpgradeDefinition(p,malformed));
    assert(p.toJson().serialize()==good);
    malformed=draft;malformed.unitId="does_not_exist";
    assert(!ed.setUpgradeDefinition(p,malformed));
    assert(p.toJson().serialize()==good);
    assert(ed.undo(p));
    assert(compileUpgrade(p.catalog,id).maxLevel==3);
    assert(ed.redo(p));
    assert(compileUpgrade(p.catalog,id).maxLevel==4);
    assert(ed.createCatalogObject(p,"Validator","Upgrade"));
    const std::string validator=ed.selected(p)->id;
    auto check=compileValidator(p.catalog,validator);
    assert(check.kind==RTSValidatorKind::UpgradeLevelAtLeast);
    assert(check.upgradeId==id);
    assert(ed.createCatalogObject(p,"Requirement","Predicate"));
    const std::string required=ed.selected(p)->id;
    assert(compileRequirement(p.catalog,required).predicate.upgradeId==id);
    assert(ed.selectCatalogId(p,"command_center"));
    assert(ed.setCatalogField(p,"researchable_upgrades",
        picojson::value(picojson::array{picojson::value(id)})));
    assert(unitResearchIds(p.catalog,"command_center")[0]==id);
    assert(ed.selectCatalogId(p,"worker"));
    assert(ed.setCatalogField(p,"requirement",picojson::value(required)));
    p.validate();
    assert(p.catalog.cloneDerived(id,"derived_upgrade"));
    assert(compileUpgrade(p.catalog,"derived_upgrade").armorPerLevel==1.5f);
    const auto references=p.catalogReferences(id);
    bool producer=false,predicate=false;
    for(const auto& r:references){
        if(r=="command_center.researchable_upgrades")producer=true;
        if(r==validator+".upgrade")predicate=true;
    }
    assert(producer && predicate);
    const auto navigator=rtsCatalogLinks(p,id);
    assert(navigator.size()>=2);
    std::string message;
    assert(!p.removeCatalogObject(id,&message));
    assert(!message.empty());
    assert(p.renameCatalogObject(id,"enhanced_blades"));
    assert(compileValidator(p.catalog,validator).upgradeId=="enhanced_blades");
    assert(unitResearchIds(p.catalog,"command_center")[0]=="enhanced_blades");
    assert(p.renameCatalogObject("ranger","forward_ranger"));
    assert(compileUpgrade(p.catalog,"enhanced_blades").unitId=="forward_ranger");
    const RTSProject persisted=RTSProject::fromJson(p.toJson());
    assert(compileUpgrade(persisted.catalog,"enhanced_blades").maxLevel==4);
    assert(persisted.catalogReferences("enhanced_blades").size()>=2);
    // Disabling the research list is undoable and does not delete the tech.
    assert(ed.selectCatalogId(p,"command_center"));
    assert(ed.setCatalogField(p,"researchable_upgrades",
                                picojson::value(picojson::array{})));
    assert(unitResearchIds(p.catalog,"command_center").empty());
    assert(ed.undo(p));
    assert(unitResearchIds(p.catalog,"command_center")[0]=="enhanced_blades");
    assert(ed.redo(p));
    assert(unitResearchIds(p.catalog,"command_center").empty());
    std::cout<<"F7 Upgrade presets, stat edits, typed requirements, rename protection and Undo/Redo passed\n";
}
