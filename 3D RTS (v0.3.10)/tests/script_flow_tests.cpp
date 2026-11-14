#include "RTSSkirmish.hpp"
#include "RTSScriptExpression.hpp"
#include "TriggerDefinition.hpp"
#include "EditorSession.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
picojson::value node(const char* type,picojson::object fields={}){
    fields["type"]=picojson::value(type);
    return picojson::value(fields);
}
picojson::value rule(const char* id,const char* event,picojson::array actions,
                     double time=0.0){
    picojson::object e{{"type",picojson::value(event)}};
    if(std::string(event)=="After")e["seconds"]=picojson::value(time);
    if(std::string(event)=="OnVariableChanged")
        e["variable"]=picojson::value("counter");
    return picojson::value(picojson::object{
        {"id",picojson::value(id)},{"event",picojson::value(e)},
        {"once",picojson::value(true)},{"actions",picojson::value(actions)}
    });
}
picojson::value compare(const char* name,const char* op,int value){
    return node("VariableCompare",{
        {"variable",picojson::value(name)},{"operator",picojson::value(op)},
        {"value",picojson::value(double(value))}
    });
}
void rejected(const RTSProject& p){
    bool fail=false;
    try{(void)parseTriggerRules(p);}
    catch(const std::invalid_argument&){fail=true;}
    assert(fail);
}
}

int main(){
    RTSProject p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    p.extensions["trigger_variables"]=picojson::value(picojson::object{
        {"counter",picojson::value(0.0)},
        {"signal",picojson::value(0.0)}
    });
    const picojson::value addTwo=node("AddVariable",{
        {"variable",picojson::value("counter")},
        {"value_expr",RTSScriptExpression::number(2)}
    });
    const picojson::value addOne=node("AddVariable",{
        {"variable",picojson::value("counter")},
        {"value",picojson::value(1.0)}
    });
    const auto nestedIf=node("IfThenElse",{
        {"conditions",picojson::value(picojson::array{compare("counter",">=",6)})},
        {"then",picojson::value(picojson::array{addOne})},
        {"else",picojson::value(picojson::array{
            node("SetVariable",{
                {"variable",picojson::value("counter")},
                {"value",picojson::value(-100.0)}
            })
        })}
    });
    const auto grantOne=node("GrantMinerals",{
        {"amount",picojson::value(1.0)}
    });
    const auto repeatFromVariable=node("Repeat",{
        {"times_expr",RTSScriptExpression::variable("counter")},
        {"actions",picojson::value(picojson::array{grantOne})}
    });
    p.extensions["triggers"]=picojson::value(picojson::array{
        rule("setup","OnMapStart",{
            node("Repeat",{
                {"times",picojson::value(3.0)},
                {"actions",picojson::value(picojson::array{addTwo})}
            }),
            nestedIf,repeatFromVariable
        }),
        rule("on_change","OnVariableChanged",{
            node("SetVariable",{
                {"variable",picojson::value("signal")},
                {"value_expr",picojson::value(picojson::object{
                    {"op",picojson::value("Add")},
                    {"left",RTSScriptExpression::variable("counter")},
                    {"right",RTSScriptExpression::number(5)}
                })}
            })
        }),
        rule("award","After",{
            node("IfThenElse",{
                {"conditions",picojson::value(picojson::array{
                    node("PlayerMineralsAtLeast",{
                        {"amount",picojson::value(220.0)}
                    })
                })},
                {"then",picojson::value(picojson::array{
                    node("GrantMinerals",{
                        {"amount_expr",RTSScriptExpression::variable("signal")}
                    })
                })},
                {"else",picojson::value(picojson::array{
                    node("GrantMinerals",{{"amount",picojson::value(-100.0)}})
                })}
            })
        },.5)
    });
    assert(parseTriggerRules(p).size()==3);
    const RTSProject parsed=RTSProject::fromJson(p.toJson());
    assert(parsed.extensions.at("triggers").serialize()==
           p.extensions.at("triggers").serialize());
    RTSSkirmish game;
    game.loadProject(p);
    assert(game.triggerDiagnostics().variables().at("counter")==7);
    const int initialMinerals=game.playerMinerals;
    assert(initialMinerals>=7);
    std::ofstream out("script_flow_fingerprint.txt",
                      std::ios::binary|std::ios::trunc);
    assert(out.is_open());
    out<<"0 "<<game.stateDigest()<<"\n";
    for(int i=1;i<=120;++i){
        game.update(1.f/60.f);
        if(i%30==0)out<<i<<" "<<game.stateDigest()<<"\n";
    }
    out.flush();assert(out.good());
    assert(game.triggerDiagnostics().variables().at("counter")==7);
    assert(game.triggerDiagnostics().variables().at("signal")==12);
    assert(game.triggerDiagnostics().rules()[1].executions==1);
    assert(game.triggerDiagnostics().rules()[2].executions==1);
    assert(game.playerMinerals==initialMinerals+12);
    assert(verifyRTSReplay(game.replayTape()));

    auto bad=p;
    auto& triggers=bad.extensions["triggers"].get<picojson::array>();
    triggers[0].get<picojson::object>()["actions"].get<picojson::array>()[0]
        .get<picojson::object>()["times"]=picojson::value(17.0);
    rejected(bad);
    bad=p;
    bad.extensions["triggers"].get<picojson::array>()[0]
        .get<picojson::object>()["actions"].get<picojson::array>()[1]
        .get<picojson::object>()["conditions"]=picojson::value(picojson::array{});
    rejected(bad);

    // The validator rejects exponential loops up front, not after they
    // have started executing in the live simulation.
    bad=p;
    picojson::value enormous=grantOne;
    for(int i=0;i<3;++i)
        enormous=node("Repeat",{
            {"times",picojson::value(16.0)},
            {"actions",picojson::value(picojson::array{enormous})}
        });
    bad.extensions["triggers"].get<picojson::array>()[0]
        .get<picojson::object>()["actions"].get<picojson::array>()[0]=enormous;
    rejected(bad);

    EditorSession editor;
    editor.panel=EditorSession::Panel::Triggers;
    editor.selectedTrigger=0;
    assert(editor.addTriggerNode(p,false,"IfThenElse"));
    assert(editor.addTriggerNode(p,false,"Repeat"));
    assert(parseTriggerRules(p).size()==3);
    assert(editor.undo(p));
    assert(editor.redo(p));
    assert(editor.renameTriggerVariable(p,"counter","score_counter"));
    const auto rewritten=parseTriggerRules(p);
    assert(rewritten[1].variable=="score_counter");
    const auto& setup=rewritten[0].actions;
    const auto& inner=setup[1].at("conditions").get<picojson::array>()[0]
        .get<picojson::object>();
    assert(inner.at("variable").get<std::string>()=="score_counter");
    const auto& dynamic=setup[2].at("times_expr").get<picojson::object>();
    assert(dynamic.at("name").get<std::string>()=="score_counter");
    assert(editor.undo(p));
    assert(parseTriggerVariables(p).count("counter")==1);
    std::cout<<"script control-flow: nested branch/repeat budget, replay, editor and rename passed\n";
}
