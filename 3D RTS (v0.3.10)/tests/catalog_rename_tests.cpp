#include "EditorSession.hpp"
#include "TriggerEngine.hpp"
#include <cassert>
#include <iostream>
static picojson::object& section(ProjectPlacement& p,const char* key) {
    p.overrides[key]=picojson::value(picojson::object());
    return p.overrides[key].get<picojson::object>();
}
int main() {
    RTSProject p=RTSProject::defaults();
    assert(!p.renameCatalogObject("missing","nope"));
    assert(!p.renameCatalogObject("ranger","ranger"));
    assert(!p.renameCatalogObject("ranger","worker"));
    assert(!p.renameCatalogObject("ranger","has space"));
    auto& ranger=p.placements.at(2);
    ranger.overrides["custom"]=picojson::value(picojson::object{
        {"text",picojson::value("ranger")}});
    section(ranger,"unit")["actor"]=picojson::value("ranger_actor");
    section(ranger,"actor")["model"]=picojson::value("procedural_soldier");
    picojson::object condition{
        {"type",picojson::value("UnitTypeCountAtLeast")},
        {"catalog",picojson::value("ranger")},{"owner",picojson::value(0.0)},
        {"count",picojson::value(1.0)}};
    picojson::object spawn{
        {"type",picojson::value("SpawnUnit")},{"catalog",picojson::value("ranger")},
        {"x",picojson::value(0.0)},{"z",picojson::value(0.0)}};
    picojson::object msg{{"type",picojson::value("ShowMessage")},{"text",picojson::value("ranger")}};
    p.extensions["triggers"]=picojson::value(picojson::array{picojson::value(picojson::object{
        {"id",picojson::value("refs")},
        {"event",picojson::value(picojson::object{{"type",picojson::value("OnMapStart")}})},
        {"conditions",picojson::value(picojson::array{picojson::value(condition)})},
        {"actions",picojson::value(picojson::array{picojson::value(spawn),picojson::value(msg)})}
    })});
    TriggerEngine::validate(p);
    assert(p.renameCatalogObject("ranger","sharpshooter"));
    assert(!p.catalog.find("ranger") && p.catalog.find("sharpshooter"));
    assert(p.catalog.find("worker")->parent=="sharpshooter");
    assert(p.placements.at(2).catalogId=="sharpshooter");
    assert(p.metadata.at("player_ranger").get<std::string>()=="sharpshooter");
    assert(p.placements.at(2).overrides.at("custom").get<picojson::object>()
        .at("text").get<std::string>()=="ranger");
    const auto& rule=p.extensions.at("triggers").get<picojson::array>()[0].get<picojson::object>();
    assert(rule.at("conditions").get<picojson::array>()[0].get<picojson::object>()
        .at("catalog").get<std::string>()=="sharpshooter");
    assert(rule.at("actions").get<picojson::array>()[0].get<picojson::object>()
        .at("catalog").get<std::string>()=="sharpshooter");
    assert(rule.at("actions").get<picojson::array>()[1].get<picojson::object>()
        .at("text").get<std::string>()=="ranger");
    assert(p.renameCatalogObject("ranger_actor","infantry_actor"));
    assert(p.catalog.find("worker_actor")->parent=="infantry_actor");
    assert(p.catalog.resolvedFields("sharpshooter").at("actor").get<std::string>()=="infantry_actor");
    assert(p.placements.at(2).overrides.at("unit").get<picojson::object>()
        .at("actor").get<std::string>()=="infantry_actor");
    assert(p.renameCatalogObject("procedural_soldier","infantry_mesh"));
    assert(p.catalog.resolvedFields("infantry_actor").at("model").get<std::string>()=="infantry_mesh");
    assert(p.placements.at(2).overrides.at("actor").get<picojson::object>()
        .at("model").get<std::string>()=="infantry_mesh");
    p.validate();TriggerEngine::validate(p);
    const auto saved=RTSProject::fromJson(p.toJson());
    assert(saved.catalog.find("infantry_mesh") && saved.catalog.find("sharpshooter"));
    RTSProject q=RTSProject::defaults();
    EditorSession edit;
    edit.panel=EditorSession::Panel::Data;
    while(!edit.selected(q) || edit.selected(q)->id!="ranger")edit.cycleObject(q,1);
    assert(edit.beginCatalogRename(q));
    edit.editText="new_ranger";
    assert(edit.finishCatalogRename(q));
    assert(edit.modified && q.catalog.find("new_ranger"));
    assert(edit.undo(q) && q.catalog.find("ranger") && !q.catalog.find("new_ranger"));
    assert(edit.redo(q) && q.catalog.find("new_ranger"));
    assert(edit.beginCatalogRename(q));
    edit.editText="worker";
    assert(!edit.finishCatalogRename(q));
    assert(edit.textEditing && edit.renamingCatalog);
    assert(q.catalog.find("new_ranger"));
    edit.cancelEdit();
    assert(!edit.textEditing && !edit.renamingCatalog);
    std::cout<<"catalog rename: reference migration, validation, JSON, editor undo/redo passed\n";
}
