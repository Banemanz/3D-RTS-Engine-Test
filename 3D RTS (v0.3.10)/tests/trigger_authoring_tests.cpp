#include "EditorSession.hpp"
#include "TriggerDefinition.hpp"
#include <cassert>
#include <iostream>
#include <string>

static picojson::array nodes(const picojson::object& rule,const char* key) {
    const auto it=rule.find(key);
    return it!=rule.end() && it->second.is<picojson::array>()
        ? it->second.get<picojson::array>() : picojson::array{};
}
static std::string kind(const picojson::value& node) {
    return node.get<picojson::object>().at("type").get<std::string>();
}
int main(){
    RTSProject project=RTSProject::defaults();
    EditorSession editor;
    editor.panel=EditorSession::Panel::Triggers;
    assert(editor.createTrigger(project));
    assert(editor.triggerCount(project)==1);
    const std::string initial=editor.triggerJSON(project,0);

    assert(editor.addTriggerNode(project,true,"PlayerMineralsAtLeast"));
    assert(editor.addTriggerNode(project,true,"UnitTypeCountAtLeast"));
    assert(nodes(editor.triggerObject(project,0),"conditions").size()==2);
    assert(editor.setTriggerNodeField(project,true,0,"amount",picojson::value(130.0)));
    assert(!editor.setTriggerNodeField(project,true,0,"amount",picojson::value(-1.0)));
    assert(editor.triggerObject(project,0).at("conditions").get<picojson::array>()[0]
        .get<picojson::object>().at("amount").get<double>()==130.0);
    assert(editor.setTriggerNodeField(project,true,1,"catalog",picojson::value("ranger")));
    assert(!editor.setTriggerNodeField(project,true,1,"catalog",picojson::value("missing_unit")));
    assert(editor.addTriggerNode(project,false,"GrantMinerals"));
    assert(editor.addTriggerNode(project,false,"SpawnUnit"));
    assert(editor.addTriggerNode(project,false,"EndMatch"));
    assert(editor.setTriggerNodeField(project,false,1,"amount",picojson::value(90.0)));
    auto actions=nodes(editor.triggerObject(project,0),"actions");
    assert(actions.size()==4);
    assert(kind(actions[0])=="ShowMessage" && kind(actions[1])=="GrantMinerals");
    assert(editor.moveTriggerNode(project,false,2,-1));
    actions=nodes(editor.triggerObject(project,0),"actions");
    assert(kind(actions[1])=="SpawnUnit" && kind(actions[2])=="GrantMinerals");
    assert(!editor.moveTriggerNode(project,false,0,-1));
    assert(editor.removeTriggerNode(project,false,1));
    assert(nodes(editor.triggerObject(project,0),"actions").size()==3);
    assert(editor.undo(project));
    assert(nodes(editor.triggerObject(project,0),"actions").size()==4);
    assert(editor.redo(project));
    assert(nodes(editor.triggerObject(project,0),"actions").size()==3);

    assert(editor.setTriggerEvent(project,"Every"));
    const auto event=editor.triggerObject(project,0).at("event").get<picojson::object>();
    assert(event.at("seconds").get<double>()==1.0);
    assert(editor.setTriggerEventField(project,"seconds",picojson::value(.25)));
    assert(!editor.setTriggerEventField(project,"seconds",picojson::value(0.0)));
    assert(editor.triggerObject(project,0).at("event")
        .get<picojson::object>().at("seconds").get<double>()==.25);
    assert(editor.setTriggerOnce(project,true));
    assert(!editor.setTriggerEvent(project,"UnknownEvent"));
    assert(!editor.setTriggerEvent(project,"OnRegionEnter")); // no region exists
    assert(!editor.addTriggerNode(project,false,"ExecuteArbitraryCode"));
    assert(!editor.setTriggerNodeField(project,false,0,"type",picojson::value("EndMatch")));
    assert(!editor.setTriggerNodeField(project,false,0,"text",picojson::value(std::string(300,'x'))));

    const auto rules=parseTriggerRules(project);
    assert(rules.size()==1 && rules[0].event=="Every");
    assert(rules[0].actions.size()==3);
    assert(rules[0].conditions.size()==2);
    const std::string beforeJSON=editor.triggerJSON(project,0);
    assert(editor.beginTriggerEdit(project));
    editor.editText=R"({"id":"broken","event":{"type":"Every","seconds":0},"actions":[{"type":"ShowMessage","text":"oops"}]})";
    assert(!editor.finishTriggerEdit(project));
    assert(editor.textEditing);
    assert(editor.triggerJSON(project,0)==beforeJSON);
    editor.cancelEdit();
    assert(editor.triggerJSON(project,0)==beforeJSON);
    const picojson::value encoded=project.toJson();
    RTSProject copy=RTSProject::fromJson(encoded);
    assert(parseTriggerRules(copy).size()==1);
    assert(editor.triggerJSON(copy,0)==beforeJSON);

    assert(editor.setTriggerEvent(project,"OnMapStart"));
    assert(editor.setTriggerOnce(project,false));
    // Remove all four actions is invalid: a trigger always needs at least one.
    while(nodes(editor.triggerObject(project,0),"actions").size()>1)
        assert(editor.removeTriggerNode(project,false,0));
    assert(!editor.removeTriggerNode(project,false,0));
    assert(nodes(editor.triggerObject(project,0),"actions").size()==1);
    assert(editor.triggerJSON(project,0)!=initial);
    std::cout<<"typed trigger authoring: schema, undo, atomic rollback and project persistence passed\n";
}
