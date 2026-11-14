#include "EditorSession.hpp"
#include "TriggerDefinition.hpp"
#include "RTSSkirmish.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
picojson::value node(const char* type,picojson::object fields={}) {
    fields["type"]=picojson::value(type);
    return picojson::value(fields);
}
picojson::value rule(const char* id,const char* event,double seconds,
                     picojson::array actions,picojson::array conditions={}) {
    picojson::object e{{"type",picojson::value(event)}};
    if(std::string(event)=="Every" || std::string(event)=="After")
        e["seconds"]=picojson::value(seconds);
    return picojson::value(picojson::object{
        {"id",picojson::value(id)},
        {"event",picojson::value(e)},
        {"once",picojson::value(std::string(event)!="Every")},
        {"conditions",picojson::value(conditions)},
        {"actions",picojson::value(actions)}
    });
}
void rejected(const RTSProject& project) {
    bool failed=false;
    try{(void)parseTriggerRules(project);}
    catch(const std::invalid_argument&){failed=true;}
    assert(failed);
}
}

int main(){
    RTSProject project=RTSProject::defaults();
    project.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    project.metadata["enemy_ai_enabled"]=picojson::value(false);
    project.extensions["trigger_variables"]=picojson::value(picojson::object{
        {"counter",picojson::value(0.0)},
        {"limit",picojson::value(5.0)}
    });
    const auto counter=picojson::object{{"variable",picojson::value("counter")}};
    picojson::object set=counter,add=counter,compare=counter,blocked=counter;
    set["value"]=picojson::value(2.0);
    add["value"]=picojson::value(1.0);
    compare["value"]=picojson::value(5.0);
    compare["operator"]=picojson::value(">=");
    blocked["value"]=picojson::value(100.0);
    blocked["operator"]=picojson::value(">");
    project.extensions["triggers"]=picojson::value(picojson::array{
        rule("initialize","OnMapStart",0.,{
            node("SetVariable",set),node("AddVariable",add)
        }),
        rule("increment","Every",.1,{node("AddVariable",add)}),
        rule("reward","After",.5,{
            node("GrantMinerals",{{"amount",picojson::value(10.0)}})
        },{node("VariableCompare",compare)}),
        rule("blocked","After",.5,{
            node("GrantMinerals",{{"amount",picojson::value(999.0)}})
        },{node("VariableCompare",blocked)})
    });
    assert(parseTriggerVariables(project).at("counter")==0);
    assert(parseTriggerRules(project).size()==4);
    project.validate();

    RTSSkirmish game;
    game.loadProject(project);
    assert(game.triggerDiagnostics().variables().at("counter")==3);
    const int initialMinerals=game.playerMinerals;
    const auto firstDigest=game.stateDigest();
    assert(firstDigest.size()==16);
    {
        std::ofstream fingerprint("trigger_globals_fingerprint.txt",
                                  std::ios::binary|std::ios::trunc);
        assert(fingerprint.is_open());
        fingerprint<<"0 "<<firstDigest<<"\n";
        for(int i=1;i<=120;++i){
            game.update(1.f/60.f);
            if(i%30==0)
                fingerprint<<i<<" "<<game.stateDigest()<<"\n";
        }
        assert(fingerprint.good());
    }
    assert(game.playerMinerals==initialMinerals+10);
    assert(game.triggerDiagnostics().variables().at("counter")>=15);
    assert(game.triggerDiagnostics().rules()[2].executions==1);
    assert(game.triggerDiagnostics().rules()[3].executions==0);
    std::string diagnostic;
    const RTSReplayTape tape=game.replayTape();
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(diagnostic.find("verified")!=std::string::npos);

    // A script variable is real authoritative state, not UI-only telemetry.
    auto modified=project;
    modified.extensions["trigger_variables"].get<picojson::object>()["limit"]=
        picojson::value(6.0);
    RTSSkirmish alternative;
    alternative.loadProject(modified);
    assert(alternative.stateDigest()!=firstDigest);
    game.reset();
    assert(game.triggerDiagnostics().variables().at("counter")==3);
    assert(game.triggerDiagnostics().rules()[2].executions==0);

    // Unknown identifiers and malformed declarations never become live scripts.
    auto invalid=project;
    invalid.extensions["trigger_variables"].get<picojson::object>()["bad-name"]=
        picojson::value(1.0);
    rejected(invalid);
    invalid=project;
    invalid.extensions["trigger_variables"].get<picojson::object>()["counter"]=
        picojson::value(1.5);
    rejected(invalid);
    invalid=project;
    invalid.extensions["trigger_variables"].get<picojson::object>()["counter"]=
        picojson::value(100000001.0);
    rejected(invalid);
    invalid=project;
    invalid.extensions["trigger_variables"].get<picojson::object>().erase("counter");
    rejected(invalid); // references cannot dangle
    invalid=project;
    auto& reward=invalid.extensions["triggers"].get<picojson::array>()[2]
        .get<picojson::object>().at("conditions").get<picojson::array>()[0]
        .get<picojson::object>();
    reward["operator"]=picojson::value("exec");
    rejected(invalid);
    invalid=project;
    reward=invalid.extensions["triggers"].get<picojson::array>()[2]
        .get<picojson::object>().at("conditions").get<picojson::array>()[0]
        .get<picojson::object>(); // ensure this test changes the actual array
    reward["value"]=picojson::value(1.25);
    rejected(invalid);

    // The visual ECA workbench uses exactly the same safe mutation layer.
    EditorSession editor;
    editor.panel=EditorSession::Panel::Triggers;
    assert(editor.addTriggerVariable(project,"editorFlag",7));
    assert(editor.setTriggerVariable(project,"editorFlag",9));
    assert(project.extensions["trigger_variables"].get<picojson::object>()
           .at("editorFlag").get<double>()==9.);
    assert(editor.undo(project));
    assert(project.extensions["trigger_variables"].get<picojson::object>()
           .at("editorFlag").get<double>()==7.);
    assert(editor.redo(project));
    assert(!editor.removeTriggerVariable(project,"counter")); // referenced
    assert(editor.removeTriggerVariable(project,"editorFlag"));
    assert(editor.addTriggerNode(project,true,"VariableCompare"));
    assert(editor.addTriggerNode(project,false,"AddVariable"));
    assert(!editor.setTriggerNodeField(project,false,2,"variable",
                                      picojson::value("missingGlobal")));
    assert(editor.setTriggerNodeField(project,false,2,"variable",
                                     picojson::value("limit")));
    assert(parseTriggerRules(project).size()==4);
    std::cout<<"trigger globals: lifecycle, ECA, bounds, undo, replay and parity passed\n";
}
