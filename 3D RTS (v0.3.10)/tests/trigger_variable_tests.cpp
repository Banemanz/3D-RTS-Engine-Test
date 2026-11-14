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
    if(std::string(event)=="OnVariableChanged")
        e["variable"]=picojson::value("counter");
    return picojson::value(picojson::object{
        {"id",picojson::value(id)},
        {"event",picojson::value(e)},
        {"once",picojson::value(std::string(event)!="Every" &&
                                 std::string(event)!="OnVariableChanged")},
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

int main(int argc,char** argv){
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
        },{node("VariableCompare",blocked)}),
        rule("watch_counter","OnVariableChanged",0.,{
            node("GrantMinerals",{{"amount",picojson::value(1.0)}})
        })
    });
    assert(parseTriggerVariables(project).at("counter")==0);
    assert(parseTriggerRules(project).size()==5);
    project.validate();
    // The authored globals and event subscriptions must survive exact
    // serializer/deserializer round-trip, including replay snapshots.
    const RTSProject roundtrip=RTSProject::fromJson(project.toJson());
    assert(parseTriggerVariables(roundtrip)==parseTriggerVariables(project));
    assert(parseTriggerRules(roundtrip).size()==5);
    assert(roundtrip.extensions.at("triggers").serialize()==
           project.extensions.at("triggers").serialize());

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
    // Each net variable change triggers exactly once on the following tick.
    const auto& watch=game.triggerDiagnostics().rules()[4];
    assert(watch.executions>=10);
    assert(game.playerMinerals==initialMinerals+10+int(watch.executions));
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
    assert(game.triggerDiagnostics().rules()[4].executions==0);

    // Multiple same-tick writes are coalesced. Returning to the starting
    // value produces no event at all (regardless of intermediate states).
    RTSProject cancelled=RTSProject::defaults();
    cancelled.metadata["enemy_ai_enabled"]=picojson::value(false);
    cancelled.extensions["trigger_variables"]=picojson::value(picojson::object{
        {"counter",picojson::value(0.0)}
    });
    picojson::object one=counter,zero=counter;
    one["value"]=picojson::value(1.0);
    zero["value"]=picojson::value(0.0);
    cancelled.extensions["triggers"]=picojson::value(picojson::array{
        rule("back_to_zero","OnMapStart",0.,{
            node("SetVariable",one),node("SetVariable",zero)
        }),
        rule("no_event","OnVariableChanged",0.,{
            node("GrantMinerals",{{"amount",picojson::value(55.0)}})
        })
    });
    RTSSkirmish noNetChange;
    noNetChange.loadProject(cancelled);
    const int beforeCoalesced=noNetChange.playerMinerals;
    noNetChange.update(1.f/60.f);
    assert(noNetChange.playerMinerals==beforeCoalesced);
    assert(noNetChange.triggerDiagnostics().rules()[1].executions==0);

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
    invalid.extensions["triggers"].get<picojson::array>()[4]
        .get<picojson::object>().at("event").get<picojson::object>()["variable"]=
        picojson::value("missingGlobal");
    rejected(invalid);
    invalid=project;
    auto& reward=invalid.extensions["triggers"].get<picojson::array>()[2]
        .get<picojson::object>().at("conditions").get<picojson::array>()[0]
        .get<picojson::object>();
    reward["operator"]=picojson::value("exec");
    rejected(invalid);
    invalid=project;
    invalid.extensions["triggers"].get<picojson::array>()[2]
        .get<picojson::object>().at("conditions").get<picojson::array>()[0]
        .get<picojson::object>()["value"]=picojson::value(1.25);
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
    assert(parseTriggerRules(project).size()==5);
    assert(editor.setTriggerEvent(project,"OnVariableChanged"));
    assert(editor.setTriggerEventField(project,"variable",picojson::value("limit")));
    assert(!editor.setTriggerEventField(project,"variable",
                                       picojson::value("missingGlobal")));
    if(argc>1) {
        // Test the actual shipped creator map, not only a synthetic test graph.
        RTSProject shipped;
        shipped.loadFile(argv[1]);
        assert(parseTriggerRules(shipped).size()==4);
        RTSSkirmish sample;
        sample.loadProject(shipped);
        for(int i=0;i<330;++i)sample.update(1.f/60.f);
        const auto& globals=sample.triggerDiagnostics().variables();
        assert(globals.at("alarm_level")>=3);
        assert(globals.at("raid_count")>=2);
        assert(sample.triggerDiagnostics().rules()[2].executions>=2);
        assert(sample.triggerDiagnostics().rules()[3].executions>=1);
        assert(verifyRTSReplay(sample.replayTape()));
    }
    std::cout<<"trigger globals: lifecycle, ECA, bounds, undo, replay and parity passed\n";
}
