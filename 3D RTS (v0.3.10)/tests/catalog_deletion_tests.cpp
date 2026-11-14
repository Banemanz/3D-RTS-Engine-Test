#include "EditorSession.hpp"
#include "TriggerEngine.hpp"
#include <cassert>
#include <iostream>
int main() {
    RTSProject p=RTSProject::defaults();
    std::string error;
    assert(!p.removeCatalogObject("missing",&error));
    assert(error.find("Unknown")!=std::string::npos);
    assert(!p.removeCatalogObject("ranger",&error));
    assert(error.find("Used by")!=std::string::npos);
    assert(!p.catalogReferences("ranger").empty());
    assert(p.catalog.cloneDerived("ranger","unused_a"));
    assert(p.catalog.cloneDerived("unused_a","child_a"));
    assert(!p.removeCatalogObject("unused_a",&error));
    assert(error.find("inherited")!=std::string::npos);
    assert(p.removeCatalogObject("child_a",&error));
    assert(p.removeCatalogObject("unused_a",&error));
    assert(!p.catalog.find("child_a") && !p.catalog.find("unused_a"));
    assert(p.catalog.find("worker"));
    assert(p.catalog.cloneDerived("ranger","unlinked"));
    assert(p.removeCatalogObject("unlinked",&error));
    assert(p.catalog.find("barracks") && p.catalog.find("ranger_actor"));
    assert(p.catalog.cloneDerived("ranger","script_spawn"));
    picojson::object script{{"id",picojson::value("spawn_example")},
      {"event",picojson::value(picojson::object{{"type",picojson::value("OnMapStart")}})},
      {"actions",picojson::value(picojson::array{picojson::value(picojson::object{
         {"type",picojson::value("SpawnUnit")},{"catalog",picojson::value("script_spawn")},
         {"x",picojson::value(0.0)},{"z",picojson::value(0.0)}
      })})}};
    p.extensions["triggers"]=picojson::value(picojson::array{picojson::value(script)});
    TriggerEngine::validate(p);
    assert(!p.removeCatalogObject("script_spawn",&error));
    assert(error.find("trigger")!=std::string::npos);
    p.extensions.erase("triggers");
    assert(p.removeCatalogObject("script_spawn",&error));
    assert(p.catalog.cloneDerived("ranger","metadata_ranger"));
    p.metadata["player_ranger"]=picojson::value("metadata_ranger");
    assert(!p.removeCatalogObject("metadata_ranger",&error));
    assert(error.find("skirmish role")!=std::string::npos);
    p.metadata.erase("player_ranger");
    assert(p.removeCatalogObject("metadata_ranger",&error));
    p.validate();

    RTSProject q=RTSProject::defaults();
    assert(q.catalog.cloneDerived("ranger","to_delete"));
    EditorSession ed;
    ed.panel=EditorSession::Panel::Data;
    ed.selectedObject=q.catalog.entries().size()-1;
    assert(ed.selected(q)->id=="to_delete");
    assert(ed.deleteCatalogObject(q));
    assert(!q.catalog.find("to_delete") && ed.modified);
    assert(ed.undo(q) && q.catalog.find("to_delete"));
    assert(ed.redo(q) && !q.catalog.find("to_delete"));
    const auto roundtrip=RTSProject::fromJson(q.toJson());
    assert(!roundtrip.catalog.find("to_delete"));

    // Erasing vector entries must repair the ID->index map used by the F7 editor.
    DataCatalog many=DataCatalog::defaults();
    for(int i=0;i<250;++i)
        assert(many.cloneDerived("ranger","v"+std::to_string(i)));
    assert(many.erase("v4"));
    assert(!many.find("v4") && many.find("v5") && many.find("v249"));
    many.validate();
    std::cout<<"catalog deletion: dependency guards, triggers, roles, editor history and hash integrity passed\n";
}
