#include "EditorSession.hpp"
#include "RTSCatalogNavigator.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Show/Use editor tests must run Release assertions"
#endif
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool has(const std::vector<std::string>& refs,const std::string& item){
    return std::find(refs.begin(),refs.end(),item)!=refs.end();
}
}
int main(){
    RTSProject p=RTSProject::defaults();
    EditorSession ed;ed.panel=EditorSession::Panel::Data;
    assert(ed.createCatalogObject(p,"Upgrade","Damage"));
    const std::string tech=ed.selected(p)->id;
    assert(ed.createCatalogObject(p,"Validator","Upgrade"));
    assert(ed.createCatalogObject(p,"Requirement","Predicate"));
    const std::string show=ed.selected(p)->id;
    assert(compileRequirement(p.catalog,show).predicate.upgradeId==tech);
    assert(ed.selectCatalogId(p,tech));
    RTSUpgradeDefinition draft=compileUpgrade(p.catalog,tech);
    draft.showRequirementId=show;
    draft.requirementId=show;
    assert(ed.setUpgradeDefinition(p,draft));
    assert(compileUpgrade(p.catalog,tech).showRequirementId==show);
    assert(ed.undo(p));
    assert(compileUpgrade(p.catalog,tech).showRequirementId.empty());
    assert(ed.redo(p));
    assert(compileUpgrade(p.catalog,tech).showRequirementId==show);
    const auto knownGood=p.toJson().serialize();
    auto bad=draft;bad.showRequirementId="missing_graph";
    assert(!ed.setUpgradeDefinition(p,bad));
    assert(p.toJson().serialize()==knownGood);
    // Target-scoped graphs cannot be evaluated for UI Show/production.
    assert(p.catalog.setField(
        compileRequirement(p.catalog,show).predicate.id,
        "subject",picojson::value("Target")));
    assert(!ed.setUpgradeDefinition(p,draft));
    // Restore the valid authoring before further editable operations.
    assert(p.catalog.setField(
        compileRequirement(p.catalog,show).predicate.id,
        "subject",picojson::value("Caster")));
    p.validate();

    assert(ed.selectCatalogId(p,"worker"));
    assert(ed.setCatalogField(p,"show_requirement",picojson::value(show)));
    assert(ed.setCatalogField(p,"requirement",picojson::value(show)));
    CatalogEntry effect;effect.type="Effect";effect.id="tech_visibility_effect";
    effect.fields={{"kind",picojson::value("Heal")},
                   {"amount",picojson::value(1.)}};
    assert(p.catalog.add(effect));
    CatalogEntry ability;ability.type="Ability";ability.id="tech_visibility_ability";
    ability.fields={{"target",picojson::value("Self")},
                    {"effect",picojson::value("tech_visibility_effect")},
                    {"show_requirement",picojson::value(show)}};
    assert(p.catalog.add(ability));
    ProjectPlacement placed;
    placed.id="visibility_instance";placed.kind="Unit";
    placed.catalogId="command_center";placed.name="Visibility Producer";
    placed.owner=0;placed.position={3.f,0.f,3.f};
    placed.overrides["unit"]=picojson::value(picojson::object{
        {"show_requirement",picojson::value(show)}});
    p.placements.push_back(placed);
    p.validate();
    const auto refs=p.catalogReferences(show);
    assert(has(refs,tech+".show_requirement"));
    assert(has(refs,tech+".requirement"));
    assert(has(refs,"worker.show_requirement"));
    assert(has(refs,"worker.requirement"));
    assert(has(refs,"tech_visibility_ability.show_requirement"));
    assert(has(refs,"placement visibility_instance.unit.show_requirement"));
    const auto edges=rtsCatalogLinks(p,show);
    int incoming=0;
    for(const auto& edge:edges)if(edge.to==show)++incoming;
    assert(incoming>=5);
    std::string reason;
    assert(!p.removeCatalogObject(show,&reason));
    assert(!reason.empty());
    assert(p.renameCatalogObject(show,"renamed_visibility"));
    const auto visible=compileUpgrade(p.catalog,tech);
    assert(visible.showRequirementId=="renamed_visibility");
    assert(visible.requirementId=="renamed_visibility");
    assert(p.catalog.resolvedFields("worker").at("show_requirement").
           get<std::string>()=="renamed_visibility");
    assert(p.catalog.resolvedFields("worker").at("requirement").
           get<std::string>()=="renamed_visibility");
    assert(compileAbility(p.catalog,"tech_visibility_ability").
           showRequirementId=="renamed_visibility");
    assert(p.placements.back().overrides.at("unit").get<picojson::object>().
           at("show_requirement").get<std::string>()=="renamed_visibility");
    const RTSProject persisted=RTSProject::fromJson(p.toJson());
    assert(compileUpgrade(persisted.catalog,tech).showRequirementId==
           "renamed_visibility");
    assert(!persisted.catalogReferences("renamed_visibility").empty());
    std::cout<<"Show/Use editor: typed references, atomic Upgrade edits, "
                "rollback, undo/redo, placement rename, graph links passed\n";
}
