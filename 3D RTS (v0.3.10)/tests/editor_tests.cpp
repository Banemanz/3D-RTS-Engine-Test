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
    assert(editor.duplicateCatalogObject(p));
    assert(p.catalog.entries().size()==DataCatalog::defaults().entries().size()+1);
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
    std::cout<<"editor: terrain strokes, placement undo, data and trigger editing passed\n";
}
