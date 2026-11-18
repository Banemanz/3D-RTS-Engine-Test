#include "EditorSession.hpp"
#include "InstanceOverrides.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Actor event authoring CTest must run Release assertions"
#endif
#include <iostream>
int main(){
    RTSProject project=RTSProject::defaults();
    EditorSession editor;editor.panel=EditorSession::Panel::Data;
    assert(editor.createCatalogObject(project,"Actor","Events"));
    const auto id=editor.selected(project)->id;
    const auto initial=compileActorCues(project.catalog,id);
    assert(initial.size()==1);
    assert(initial[0].event==RTSActorCueEvent::Attack);
    auto draft=initial;
    RTSActorCueDefinition death;
    death.event=RTSActorCueEvent::Death;
    death.style=RTSActorCueStyle::Beacon;
    death.tint={1.f,.2f,.1f};
    death.radius=2.75f;death.durationTicks=80;
    draft.push_back(death);
    assert(editor.setActorCues(project,draft));
    const auto after=project.toJson().serialize();
    assert(compileActorCues(project.catalog,id).size()==2);
    auto invalid=draft;invalid[0].radius=-.5f;
    assert(!editor.setActorCues(project,invalid));
    assert(project.toJson().serialize()==after);
    invalid=draft;invalid[0].durationTicks=0;
    assert(!editor.setActorCues(project,invalid));
    assert(project.toJson().serialize()==after);
    invalid=draft;invalid[0].tint.x=5.f;
    assert(!editor.setActorCues(project,invalid));
    assert(project.toJson().serialize()==after);
    assert(editor.undo(project));
    assert(compileActorCues(project.catalog,id).size()==1);
    assert(editor.redo(project));
    assert(compileActorCues(project.catalog,id).size()==2);
    assert(project.catalog.cloneDerived(id,"event_inherited_actor"));
    project.validate();
    assert(compileActorCues(project.catalog,"event_inherited_actor").size()==2);
    assert(project.catalog.setField("event_inherited_actor","events",
                                    picojson::value(picojson::array{})));
    project.validate();
    assert(compileActorCues(project.catalog,"event_inherited_actor").empty());
    const auto roundtrip=RTSProject::fromJson(project.toJson());
    assert(compileActorCues(roundtrip.catalog,id).size()==2);
    assert(compileActorCues(roundtrip.catalog,"event_inherited_actor").empty());
    assert(editor.selectCatalogId(project,id));
    assert(editor.setActorCues(project,{}));
    assert(compileActorCues(project.catalog,id).empty());
    assert(editor.undo(project));
    assert(compileActorCues(project.catalog,id).size()==2);
    assert(editor.redo(project));
    assert(compileActorCues(project.catalog,id).empty());
    std::cout<<"F7 Actor event presets, grouped edits, validation, inheritance and undo/redo passed\n";
}
