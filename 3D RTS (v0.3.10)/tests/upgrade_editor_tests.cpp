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
    // One placed producer can override its research list; typed Upgrade
    // rename/delete must track these references too.
    ProjectPlacement placed;
    placed.id="research_instance";placed.kind="Unit";
    placed.catalogId="command_center";placed.owner=0;
    placed.name="Research Instance";placed.position={0.f,0.f,0.f};
    placed.overrides["unit"]=picojson::value(picojson::object{
        {"researchable_upgrades",picojson::value(picojson::array{
            picojson::value(id)})}});
    p.placements.push_back(placed);
    p.validate();
    const auto references=p.catalogReferences(id);
    bool producer=false,predicate=false;
    for(const auto& r:references){
        if(r=="command_center.researchable_upgrades")producer=true;
        if(r==validator+".upgrade")predicate=true;
    }
    assert(producer && predicate);
    bool placementRef=false;
    for(const auto& r:references)
        if(r=="placement research_instance.unit.researchable_upgrades")
            placementRef=true;
    assert(placementRef);
    const auto navigator=rtsCatalogLinks(p,id);
    assert(navigator.size()>=2);
    std::string message;
    assert(!p.removeCatalogObject(id,&message));
    assert(!message.empty());
    assert(p.renameCatalogObject(id,"enhanced_blades"));
    const auto& local=p.placements.back().overrides.at("unit").
        get<picojson::object>().at("researchable_upgrades").get<picojson::array>();
    assert(local.size()==1 && local[0].get<std::string>()=="enhanced_blades");
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
    // Level table transactions: grouped undo, strict validation, inheritance
    // and clearing an inherited table with a local empty override.
    RTSProject levelsProject=RTSProject::defaults();
    EditorSession levelsEditor;levelsEditor.panel=EditorSession::Panel::Data;
    assert(levelsEditor.createCatalogObject(levelsProject,"Upgrade","Damage"));
    const std::string customId=levelsEditor.selected(levelsProject)->id;
    auto custom=compileUpgrade(levelsProject.catalog,customId);
    assert(custom.levels.size()==3 && !custom.customLevels);
    custom.customLevels=true;
    custom.levels[0].mineralCost=20;
    custom.levels[1].researchTicks=420;
    custom.levels[2].damageBonus=5.f;
    assert(levelsEditor.setUpgradeDefinition(levelsProject,custom));
    const auto saved=levelsProject.toJson().serialize();
    assert(compileUpgrade(levelsProject.catalog,customId).levels[1].researchTicks==420);
    auto badCustom=custom;
    badCustom.levels[0].researchTicks=0;
    assert(!levelsEditor.setUpgradeDefinition(levelsProject,badCustom));
    assert(levelsProject.toJson().serialize()==saved);
    badCustom=custom;badCustom.levels.pop_back();
    assert(!levelsEditor.setUpgradeDefinition(levelsProject,badCustom));
    assert(levelsProject.toJson().serialize()==saved);
    assert(levelsEditor.undo(levelsProject));
    assert(!compileUpgrade(levelsProject.catalog,customId).customLevels);
    assert(levelsEditor.redo(levelsProject));
    assert(compileUpgrade(levelsProject.catalog,customId).customLevels);
    assert(levelsProject.catalog.cloneDerived(customId,"derived_level_tech"));
    assert(levelsEditor.selectCatalogId(levelsProject,"derived_level_tech"));
    auto inherited=compileUpgrade(levelsProject.catalog,"derived_level_tech");
    assert(inherited.customLevels && inherited.levels[0].mineralCost==20);
    inherited.customLevels=false;
    assert(levelsEditor.setUpgradeDefinition(levelsProject,inherited));
    const auto uniform=compileUpgrade(levelsProject.catalog,"derived_level_tech");
    assert(!uniform.customLevels && uniform.levels[0].mineralCost==uniform.mineralCost);
    assert(compileUpgrade(levelsProject.catalog,customId).customLevels);
    assert(levelsEditor.undo(levelsProject));
    assert(compileUpgrade(levelsProject.catalog,"derived_level_tech").customLevels);
    std::cout<<"F7 Upgrade table editing, inherited overrides and Undo/Redo passed\n";
}
