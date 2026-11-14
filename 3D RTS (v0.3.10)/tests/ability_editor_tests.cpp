#include "EditorSession.hpp"
#include "DataCatalog.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Ability editor CTest requires live assertions in Release"
#endif
#include <iostream>
#include <string>

int main(){
    RTSProject project=RTSProject::defaults();
    EditorSession editor;
    editor.panel=EditorSession::Panel::Data;
    const std::size_t initial=project.catalog.entries().size();
    assert(editor.createCatalogObject(project,"Ability","Self"));
    assert(editor.selected(project));
    const std::string self=editor.selected(project)->id;
    assert(compileAbility(project.catalog,self).target==AbilityTarget::Self);
    assert(editor.createCatalogObject(project,"Ability","Enemy"));
    const std::string enemy=editor.selected(project)->id;
    assert(self!=enemy);
    assert(editor.setCatalogField(project,"effect",picojson::value("kinetic_pulse")));
    assert(editor.setCatalogField(project,"cast_range",picojson::value(9.0)));
    assert(editor.setCatalogField(project,"mineral_cost",picojson::value(10.0)));
    assert(editor.setCatalogField(project,"cooldown_ticks",picojson::value(90.0)));
    const auto def=compileAbility(project.catalog,enemy);
    assert(def.target==AbilityTarget::Enemy && def.effect=="kinetic_pulse");
    assert(def.mineralCost==10 && def.cooldownTicks==90);
    assert(!editor.setCatalogField(project,"cast_range",picojson::value(-8.0)));
    assert(compileAbility(project.catalog,enemy).castRange==9.f);
    assert(!editor.setCatalogField(project,"target",picojson::value("Terrain")));
    assert(compileAbility(project.catalog,enemy).target==AbilityTarget::Enemy);
    assert(!editor.createCatalogObject(project,"Ability","Invalid"));

    assert(editor.selectCatalogId(project,"ranger"));
    assert(editor.setCatalogField(project,"abilities",picojson::value(
        picojson::array{picojson::value(self),picojson::value(enemy)})));
    const auto assigned=unitAbilityIds(project.catalog,"ranger");
    assert(assigned.size()==2 && assigned[0]==self && assigned[1]==enemy);
    assert(!editor.setCatalogField(project,"abilities",picojson::value(
        picojson::array{picojson::value(self),picojson::value(self)})));
    assert(unitAbilityIds(project.catalog,"ranger").size()==2);
    std::string why;
    assert(!project.removeCatalogObject(enemy,&why));
    assert(why.find("abilities")!=std::string::npos);
    assert(!project.removeCatalogObject("kinetic_pulse",&why));
    assert(why.find("effect")!=std::string::npos);

    assert(editor.selectCatalogId(project,enemy));
    assert(editor.beginCatalogRename(project));
    editor.editText="renamed_energy_bolt";
    assert(editor.finishCatalogRename(project));
    assert(unitAbilityIds(project.catalog,"ranger")[1]=="renamed_energy_bolt");
    assert(editor.undo(project));
    assert(unitAbilityIds(project.catalog,"ranger")[1]==enemy);
    assert(editor.redo(project));
    assert(unitAbilityIds(project.catalog,"ranger")[1]=="renamed_energy_bolt");

    assert(editor.selectCatalogId(project,"ranger"));
    assert(editor.setCatalogField(project,"abilities",picojson::value(picojson::array{})));
    assert(!project.catalogReferences(self).size());
    assert(project.removeCatalogObject(self));
    assert(project.catalog.entries().size()==initial+1);
    const auto roundtrip=RTSProject::fromJson(project.toJson());
    assert(roundtrip.catalog.entries().size()==project.catalog.entries().size());
    assert(compileAbility(roundtrip.catalog,"renamed_energy_bolt").effect=="kinetic_pulse");
    std::cout<<"ability authoring: F7 presets, fields, list assignment, ref migration, undo, serialization passed\n";
}
