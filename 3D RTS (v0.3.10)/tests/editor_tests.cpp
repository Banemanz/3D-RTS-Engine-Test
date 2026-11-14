#include "EditorSession.hpp"
#include <cassert>
#include <iostream>
int main() {
    RTSProject p=RTSProject::defaults();
    EditorSession editor;
    editor.panel=EditorSession::Panel::Terrain;
    editor.startStroke(p);
    assert(editor.paint(p,{0,0,0}));
    editor.finishStroke(p);
    assert(p.terrain.sampleHeight(0,0)>0);
    assert(editor.undo(p));
    assert(p.terrain.sampleHeight(0,0)==0);
    assert(editor.redo(p));
    assert(p.terrain.sampleHeight(0,0)>0);
    editor.tool=EditorSession::Tool::PlaceMineral;
    std::size_t start=p.placements.size();
    assert(editor.place(p,{6,0,5}));
    assert(p.placements.size()==start+1);
    assert(editor.undo(p) && p.placements.size()==start);
    assert(editor.redo(p) && p.placements.size()==start+1);
    editor.panel=EditorSession::Panel::Data;
    while(!editor.selected(p) || editor.selected(p)->id!="marine")
        editor.cycleObject(p,1);
    auto fields=editor.fields(p);
    while(editor.selectedFieldName(p)!="damage")editor.cycleField(p,1);
    assert(editor.beginValueEdit(p));
    editor.editText="34";
    assert(editor.finishEdit(p));
    assert(unitFromCatalog(p.catalog,"marine").damage==34.0f);
    assert(editor.undo(p));
    assert(unitFromCatalog(p.catalog,"marine").damage==7.0f);
    assert(editor.redo(p));
    assert(unitFromCatalog(p.catalog,"marine").damage==34.0f);
    assert(editor.duplicateCatalogObject(p));
    assert(p.catalog.entries().size()==DataCatalog::defaults().entries().size()+1);
    assert(editor.undo(p));
    assert(p.catalog.entries().size()==DataCatalog::defaults().entries().size());
    assert(editor.redo(p));
    assert(p.catalog.entries().size()==DataCatalog::defaults().entries().size()+1);
    editor.beginCatalogSearch();
    editor.editText="mArInE";
    assert(editor.finishCatalogSearch(p));
    assert(editor.matchingCatalogCount(p)>=2);
    assert(editor.selected(p)->id.find("marine")!=std::string::npos);
    editor.cycleObject(p,1);
    assert(editor.catalogMatches(*editor.selected(p)));
    editor.beginCatalogSearch();
    editor.editText="no_such_rts_object_123";
    assert(!editor.finishCatalogSearch(p));
    assert(editor.catalogFilter=="mArInE");
    editor.clearCatalogSearch(p);
    assert(editor.catalogFilter.empty());
    assert(editor.matchingCatalogCount(p)==p.catalog.entries().size());
    editor.beginCatalogSearch();
    editor.editText="Unit";
    assert(editor.finishCatalogSearch(p));
    assert(editor.selected(p)->type=="Unit");
    editor.clearCatalogSearch(p);
    // Galaxy-style inspector navigation follows stable IDs even when a
    // search filter would otherwise hide the linked actor/model.
    assert(editor.selectCatalogId(p,"worker"));
    editor.catalogFilter="only_something_else";
    assert(editor.selectCatalogId(p,"marine_actor"));
    assert(editor.catalogFilter.empty());
    assert(editor.selectCatalogId(p,"procedural_soldier"));
    assert(editor.selected(p)->type=="Model");
    assert(!editor.selectCatalogId(p,"does_not_exist"));
    assert(editor.selected(p)->id=="procedural_soldier");
    assert(editor.selectCatalogId(p,"worker"));
    // Typed widgets commit exactly one validated undo snapshot.
    assert(!p.catalog.find("worker")->fields.count("armor"));
    assert(editor.setCatalogField(p,"armor",picojson::value(2.0)));
    assert(unitFromCatalog(p.catalog,"worker").armor==2.f);
    assert(editor.undo(p));
    assert(!p.catalog.find("worker")->fields.count("armor"));
    assert(editor.redo(p));
    assert(unitFromCatalog(p.catalog,"worker").armor==2.f);
    assert(!editor.setCatalogField(p,"actor",picojson::value("missing_actor")));
    assert(p.catalog.resolvedFields("worker").at("actor").get<std::string>()=="worker_actor");
    assert(editor.setCatalogField(p,"is_worker",picojson::value(false)));
    assert(!p.catalog.resolvedFields("worker").at("is_worker").get<bool>());
    assert(editor.undo(p));
    assert(p.catalog.resolvedFields("worker").at("is_worker").get<bool>());
    editor.beginNewField();
    editor.editText="custom_mod_value";assert(editor.finishEdit(p));
    editor.editText="{\"foo\":true}";assert(editor.finishEdit(p));
    assert(p.catalog.resolvedFields(editor.selected(p)->id).count("custom_mod_value")==1);
    editor.panel=EditorSession::Panel::Triggers;
    assert(editor.triggerCount(p)==0);
    assert(editor.createTrigger(p));
    assert(editor.triggerCount(p)==1);
    assert(editor.beginTriggerEdit(p));
    editor.editText=R"({"id":"custom_rule","event":{"type":"OnMapStart"},"once":true,"actions":[{"type":"ShowMessage","text":"HELLO"}]})";
    assert(editor.finishTriggerEdit(p));
    assert(editor.triggerLabel(p,0).find("custom_rule")!=std::string::npos);
    assert(editor.duplicateTrigger(p));
    assert(editor.triggerCount(p)==2);
    assert(editor.deleteTrigger(p));
    assert(editor.triggerCount(p)==1);
    assert(editor.undo(p));
    assert(editor.triggerCount(p)==2);
    assert(editor.redo(p));
    assert(editor.triggerCount(p)==1);

    // Mixed history is chronological: trigger edit -> data edit -> terrain.
    assert(editor.undo(p));
    assert(editor.undo(p));
    assert(editor.triggerCount(p)==1);
    std::cout<<"editor: terrain strokes, placement undo, data search/filter and trigger editing passed\n";
}
