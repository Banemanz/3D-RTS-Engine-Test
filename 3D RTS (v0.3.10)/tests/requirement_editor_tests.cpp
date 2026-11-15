#include "EditorSession.hpp"
#include "RTSCatalogNavigator.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Requirement editor tests must run active Release assertions"
#endif
#include <iostream>
int main(){
    RTSProject p=RTSProject::defaults();
    EditorSession editor;editor.panel=EditorSession::Panel::Data;
    assert(editor.createCatalogObject(p,"Validator","Health"));
    const std::string validator=editor.selected(p)->id;
    auto draft=compileValidator(p.catalog,validator);
    assert(draft.kind==RTSValidatorKind::HealthBelowPercent);
    draft.subject=RTSValidatorSubject::Target;
    draft.value=27.f;
    assert(editor.setValidatorDefinition(p,draft));
    assert(compileValidator(p.catalog,validator).value==27.f);
    auto invalid=draft;invalid.value=101.f;
    const auto clean=p.toJson().serialize();
    assert(!editor.setValidatorDefinition(p,invalid));
    assert(clean==p.toJson().serialize());
    assert(editor.undo(p));
    assert(compileValidator(p.catalog,validator).value==50.f);
    assert(editor.redo(p));
    assert(compileValidator(p.catalog,validator).value==27.f);

    assert(editor.createCatalogObject(p,"Requirement","Predicate"));
    const std::string leaf=editor.selected(p)->id;
    assert(compileRequirement(p.catalog,leaf).predicate.id==validator);
    // Create a second Validator and leaf to exercise a multi-node graph.
    assert(editor.createCatalogObject(p,"Validator","Energy"));
    const std::string energy=editor.selected(p)->id;
    assert(editor.createCatalogObject(p,"Requirement","Predicate"));
    const std::string leafEnergy=editor.selected(p)->id;
    assert(editor.setRequirementDefinition(p,RTSRequirementKind::Predicate,
                                           energy,{}));
    assert(editor.createCatalogObject(p,"Requirement","All"));
    const std::string root=editor.selected(p)->id;
    assert(editor.setRequirementDefinition(p,RTSRequirementKind::All,"",
                                           {leaf,leafEnergy}));
    assert(compileRequirement(p.catalog,root).children.size()==2);
    const auto serialized=p.toJson().serialize();
    assert(!editor.setRequirementDefinition(p,RTSRequirementKind::All,"",
                                            {leaf,leaf}));
    assert(p.toJson().serialize()==serialized);
    assert(editor.undo(p));
    assert(compileRequirement(p.catalog,root).children.size()==1);
    assert(editor.redo(p));
    assert(compileRequirement(p.catalog,root).children.size()==2);
    assert(editor.selectCatalogId(p,"ranger"));
    assert(editor.setCatalogField(p,"requirement",picojson::value(root)));
    assert(p.catalog.cloneDerived("ranger","ranger_requirement_child"));
    p.validate();
    const auto links=rtsCatalogLinks(p,root);
    bool backward=false,inherited=false;
    for(const auto& link:links){
        if(link.from=="ranger" && link.to==root)backward=true;
        if(link.from=="ranger_requirement_child" && link.to==root &&
           link.inherited)inherited=true;
    }
    assert(backward && inherited);
    std::string why;
    assert(!p.removeCatalogObject(root,&why));
    assert(why.find("requirement")!=std::string::npos);
    assert(p.renameCatalogObject(root,"renamed_gate"));
    auto fields=p.catalog.resolvedFields("ranger");
    assert(fields.at("requirement").get<std::string>()=="renamed_gate");
    assert(p.renameCatalogObject(validator,"renamed_health"));
    assert(compileRequirement(p.catalog,"renamed_gate").children[0].
           predicate.id=="renamed_health");
    assert(!p.removeCatalogObject(leaf,&why));
    assert(why.find("children")!=std::string::npos);
    const auto copy=RTSProject::fromJson(p.toJson());
    assert(compileRequirement(copy.catalog,"renamed_gate").children.size()==2);
    assert(editor.selectCatalogId(p,"ranger_requirement_child"));
    assert(editor.setCatalogField(p,"requirement",picojson::value("")));
    assert(p.catalog.resolvedFields("ranger_requirement_child").
           at("requirement").get<std::string>().empty());
    assert(editor.undo(p));
    assert(p.catalog.resolvedFields("ranger_requirement_child").
           at("requirement").get<std::string>()=="renamed_gate");
    assert(editor.redo(p));
    std::cout<<"F7 Validator/Requirement graph presets, rollback, references, inheritance and history passed\n";
}
