#include "RTSScriptExpression.hpp"
#include "RTSSkirmish.hpp"
#include "EditorSession.hpp"
#include "TriggerDefinition.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "This CTest target requires live assertions in every build configuration"
#endif
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
picojson::value val(const char* type,picojson::object props={}){
    props["type"]=picojson::value(type);
    return picojson::value(props);
}
picojson::value bin(const char* op,picojson::value lhs,picojson::value rhs){
    return picojson::value(picojson::object{
        {"op",picojson::value(op)},{"left",std::move(lhs)},{"right",std::move(rhs)}
    });
}
picojson::value cmp(const char* name,const char* op,picojson::value expression){
    return val("VariableCompare",{
        {"variable",picojson::value(name)},{"operator",picojson::value(op)},
        {"value_expr",std::move(expression)}
    });
}
picojson::value rule(const char* id,const char* event,picojson::array actions,
                     picojson::array conditions={},double delay=0){
    picojson::object ev{{"type",picojson::value(event)}};
    if(std::string(event)=="After")ev["seconds"]=picojson::value(delay);
    if(std::string(event)=="OnVariableChanged")ev["variable"]=picojson::value("score");
    return picojson::value(picojson::object{
        {"id",picojson::value(id)},{"event",picojson::value(ev)},
        {"once",picojson::value(true)},
        {"conditions",picojson::value(conditions)},
        {"actions",picojson::value(actions)}
    });
}
bool invalid(const RTSProject& project){
    try{(void)parseTriggerRules(project);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
}

int main(){
    const std::map<std::string,int> state{
        {"base",3},{"multiplier",4},{"score",0},{"zero",0},
        {"huge",RTSScriptExpression::MaxValue},{"negative",-7}
    };
    using namespace RTSScriptExpression;
    const auto b=variable("base");
    const auto m=variable("multiplier");
    const auto big=variable("huge");
    const auto neg=variable("negative");
    const auto zero=variable("zero");

    assert(evaluate(bin("Add",b,m),state)==7);
    assert(evaluate(bin("Subtract",b,m),state)==-1);
    assert(evaluate(bin("Multiply",b,m),state)==12);
    assert(evaluate(bin("Divide",number(9),number(2)),state)==4);
    assert(evaluate(bin("Modulo",number(9),number(2)),state)==1);
    assert(evaluate(bin("Divide",number(9),zero),state)==0);
    assert(evaluate(bin("Modulo",number(9),zero),state)==0);
    assert(evaluate(bin("Add",big,number(1)),state)==MaxValue);
    assert(evaluate(bin("Multiply",big,big),state)==MaxValue);
    assert(evaluate(bin("Subtract",neg,big),state)==-MaxValue);
    assert(evaluate(bin("Min",b,m),state)==3);
    assert(evaluate(bin("Max",b,m),state)==4);
    assert(evaluate(picojson::value(picojson::object{
        {"op",picojson::value("Negate")},{"operand",neg}}),state)==7);
    assert(evaluate(picojson::value(picojson::object{
        {"op",picojson::value("Absolute")},{"operand",neg}}),state)==7);
    assert(evaluate(picojson::value(picojson::object{
        {"op",picojson::value("Clamp")},{"value",big},
        {"minimum",number(-10)},{"maximum",number(10)}}),state)==10);
    assert(evaluate(picojson::value(picojson::object{
        {"op",picojson::value("Clamp")},{"value",number(2)},
        {"minimum",number(10)},{"maximum",number(-10)}}),state)==2);

    auto expression=bin("Multiply",b,m);
    validate(expression,state);
    auto deep=number(0);
    for(int i=0;i<MaxDepth;++i)
        deep=picojson::value(picojson::object{
            {"op",picojson::value("Negate")},{"operand",deep}});
    bool rejected=false;
    try{validate(deep,state);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    rejected=false;
    try{validate(variable("missing"),state);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    rejected=false;
    try{validate(bin("Execute",b,m),state);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    rejected=false;
    try{validate(picojson::value(picojson::object{
        {"op",picojson::value("Literal")},{"value",picojson::value(2.5)}}),state);
    }catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);

    RTSProject project=RTSProject::defaults();
    project.metadata["enemy_ai_enabled"]=picojson::value(false);
    project.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    project.extensions["trigger_variables"]=picojson::value(picojson::object{
        {"base",picojson::value(3.0)},
        {"multiplier",picojson::value(4.0)},
        {"score",picojson::value(0.0)}
    });
    project.extensions["triggers"]=picojson::value(picojson::array{
        rule("start","OnMapStart",{
            val("SetVariable",{
                {"variable",picojson::value("score")},
                {"value_expr",bin("Multiply",b,m)}
            })
        }),
        rule("add","After",{
            val("AddVariable",{
                {"variable",picojson::value("score")},
                {"value",picojson::value(4.0)}
            })
        },{},.1),
        rule("reward","OnVariableChanged",{
            val("GrantMinerals",{
                {"amount_expr",bin("Multiply",variable("score"),number(2))}
            })
        },{cmp("score",">=",bin("Add",m,number(10)))})
    });
    assert(parseTriggerRules(project).size()==3);
    const RTSProject fromDisk=RTSProject::fromJson(project.toJson());
    assert(fromDisk.extensions.at("triggers").serialize()==
           project.extensions.at("triggers").serialize());

    RTSSkirmish game;
    game.loadProject(project);
    assert(game.triggerDiagnostics().variables().at("score")==12);
    const int beginningMinerals=game.playerMinerals;
    std::ofstream fingerprint("script_expression_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<game.stateDigest()<<"\n";
    for(int tick=1;tick<=120;++tick){
        game.update(1.f/60.f);
        if(tick%30==0)fingerprint<<tick<<" "<<game.stateDigest()<<"\n";
    }
    fingerprint.flush();
    assert(fingerprint.good());
    assert(game.triggerDiagnostics().variables().at("score")==16);
    assert(game.triggerDiagnostics().rules()[2].executions==1);
    assert(game.playerMinerals==beginningMinerals+32);
    assert(verifyRTSReplay(game.replayTape()));

    // Strict schema: ambiguous literals, invalid expression references and
    // malformed arithmetic are errors before entering the simulation.
    auto bad=project;
    auto& action0=bad.extensions["triggers"].get<picojson::array>()[0]
        .get<picojson::object>()["actions"].get<picojson::array>()[0]
        .get<picojson::object>();
    action0["value"]=picojson::value(1.0);
    assert(invalid(bad));
    bad=project;
    bad.extensions["triggers"].get<picojson::array>()[0]
        .get<picojson::object>()["actions"].get<picojson::array>()[0]
        .get<picojson::object>()["value_expr"]=variable("unknown");
    assert(invalid(bad));

    EditorSession editor;
    editor.panel=EditorSession::Panel::Triggers;
    editor.selectedTrigger=1; // AddVariable currently holds a literal
    assert(editor.setTriggerNodeOperand(project,false,0,true,"base"));
    assert(parseTriggerRules(project).size()==3);
    assert(!editor.setTriggerNodeOperand(project,false,0,true,"missing"));
    assert(editor.undo(project));
    assert(project.extensions["triggers"].get<picojson::array>()[1]
           .get<picojson::object>()["actions"].get<picojson::array>()[0]
           .get<picojson::object>().count("value")==1);
    assert(editor.redo(project));
    assert(editor.renameTriggerVariable(project,"base","basis"));
    auto migrated=parseTriggerRules(project);
    const auto& startExpr=migrated[0].actions[0].at("value_expr").get<picojson::object>();
    assert(startExpr.at("left").get<picojson::object>()
           .at("name").get<std::string>()=="basis");
    assert(migrated[1].actions[0].at("value_expr").get<picojson::object>()
           .at("name").get<std::string>()=="basis");
    assert(!editor.removeTriggerVariable(project,"basis"));
    assert(editor.undo(project));
    assert(parseTriggerVariables(project).count("base")==1);
    std::cout<<"script expressions: safe math, authored ECA, replay, rename, undo passed\n";
}
