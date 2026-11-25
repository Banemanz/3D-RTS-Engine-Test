#include "EditorSession.hpp"
#include "TriggerDefinition.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Catalog creation tests require active assertions"
#endif
#include <iostream>
#include <set>

int main(){
    RTSProject project=RTSProject::defaults();
    EditorSession ed;
    ed.panel=EditorSession::Panel::Data;
    const std::size_t start=project.catalog.entries().size();
    const std::pair<const char*,const char*> presets[]={
        {"Model","Procedural"},{"Actor","Visual"},
        {"Unit","Combat"},{"Unit","Worker"},{"Unit","Structure"},
        {"Effect","Damage"},{"Effect","Heal"},
        {"Effect","PeriodicDamage"},{"Effect","PeriodicHeal"},
        {"Effect","Sequence"}
    };
    std::set<std::string> made;
    for(const auto& item:presets){
        assert(ed.createCatalogObject(project,item.first,item.second));
        const auto* entry=ed.selected(project);
        assert(entry && entry->type==item.first);
        assert(entry->parent.empty()); // original object, not a derived clone
        assert(made.insert(entry->id).second);
        if(entry->type=="Unit"){
            const auto fields=project.catalog.resolvedFields(entry->id);
            assert(fields.count("hp_regen"));
            (void)unitFromCatalog(project.catalog,entry->id);
        }
        if(entry->type=="Effect")
            assert(!compileEffect(project.catalog,entry->id).empty());
        project.validate();
    }
    assert(project.catalog.entries().size()==start+10);
    assert(ed.createCatalogObject(project,"Effect","PeriodicDamage"));
    assert(ed.selected(project)->id!="new_effect");
    assert(!ed.createCatalogObject(project,"Invalid","Anything"));
    assert(!ed.createCatalogObject(project,"Effect","Unsupported"));
    assert(ed.undo(project)); // remove the extra periodic object
    assert(project.catalog.entries().size()==start+10);
    assert(ed.redo(project));
    assert(project.catalog.entries().size()==start+11);
    const auto copy=RTSProject::fromJson(project.toJson());
    assert(copy.catalog.entries().size()==project.catalog.entries().size());
    assert(ed.convertEffectKind(project,"Heal"));
    assert(compileEffect(project.catalog,ed.selected(project)->id)[0].kind==
           EffectOperationKind::Heal);
    assert(ed.convertEffectKind(project,"PeriodicHeal"));
    assert(compileEffect(project.catalog,ed.selected(project)->id)[0].kind==
           EffectOperationKind::PeriodicHeal);
    assert(ed.convertEffectKind(project,"Sequence"));
    assert(!compileEffect(project.catalog,ed.selected(project)->id).empty());
    assert(ed.convertEffectKind(project,"Damage"));
    assert(!ed.convertEffectKind(project,"BadOperation"));
    assert(!ed.convertEffectKind(project,"Damage"));
    assert(ed.undo(project));
    assert(ed.redo(project));
    // New objects remain valid after save/load, can be edited and are allowed
    // to be removed while they have no references.
    assert(ed.setCatalogField(project,"amount",picojson::value(12.0)));
    assert(compileEffect(project.catalog,ed.selected(project)->id)[0].amount==12.f);
    assert(ed.deleteCatalogObject(project));
    assert(project.catalog.entries().size()==start+10);
    std::cout<<"catalog creation: 10 native templates, F7 undo, validation and roundtrip passed\n";
}
