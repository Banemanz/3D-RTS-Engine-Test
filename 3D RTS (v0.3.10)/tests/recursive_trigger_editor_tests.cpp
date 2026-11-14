#include "EditorSession.hpp"
#include "TriggerTree.hpp"
#include "TriggerDefinition.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Recursive ECA authoring tests require assertions in Release"
#endif
#include <iostream>

namespace {
picojson::value node(const char* type,picojson::object fields={}){
    fields["type"]=picojson::value(type);
    return picojson::value(fields);
}
}
int main(){
    RTSProject p=RTSProject::defaults();
    p.extensions["trigger_variables"]=picojson::value(picojson::object{
        {"score",picojson::value(4.0)},
        {"bonus",picojson::value(2.0)}
    });
    p.extensions["triggers"]=picojson::value(picojson::array{
        picojson::value(picojson::object{
            {"id",picojson::value("tree_demo")},
            {"event",picojson::value(picojson::object{
                {"type",picojson::value("OnMapStart")}
            })},
            {"once",picojson::value(true)},
            {"actions",picojson::value(picojson::array{
                node("ShowMessage",{{"text",picojson::value("TREE START")}})
            })}
        })
    });
    EditorSession editor;
    editor.panel=EditorSession::Panel::Triggers;
    editor.selectedTrigger=0;
    assert(editor.addTriggerTreeNode(p,{},"actions","IfThenElse"));
    const TriggerTreePath branch{{"actions",1}};
    assert(editor.addTriggerTreeNode(p,branch,"conditions","VariableCompare"));
    assert(editor.addTriggerTreeNode(p,branch,"then","Repeat"));
    const TriggerTreePath loop{{"actions",1},{"then",1}};
    assert(editor.addTriggerTreeNode(p,loop,"actions","AddVariable"));
    const TriggerTreePath nested{{"actions",1},{"then",1},{"actions",1}};
    assert(editor.setTriggerTreeField(
        p,nested,"value",picojson::value(5.0)));
    assert(editor.setTriggerTreeOperand(p,nested,true,"score"));
    assert(!editor.setTriggerTreeField(
        p,nested,"type",picojson::value("GrantMinerals")));
    const auto ast=picojson::value(picojson::object{
        {"op",picojson::value("Multiply")},
        {"left",RTSScriptExpression::variable("score")},
        {"right",RTSScriptExpression::number(3)}
    });
    assert(editor.setTriggerExpressionNode(p,nested,"value_expr",{},ast));
    assert(editor.setTriggerExpressionNode(
        p,nested,"value_expr",{"right"},RTSScriptExpression::number(6)));
    assert(!editor.setTriggerExpressionNode(
        p,nested,"value_expr",{"right","bogus"},RTSScriptExpression::number(7)));
    assert(editor.duplicateTriggerTreeNode(p,nested));
    assert(editor.moveTriggerTreeNode(p,nested,1));
    assert(editor.removeTriggerTreeNode(p,nested));
    assert(editor.setTriggerTreeOperand(p,loop,true,"bonus"));
    assert(editor.setTriggerExpressionNode(p,loop,"times_expr",{},
        RTSScriptExpression::number(2)));
    assert(editor.addTriggerTreeNode(p,branch,"else","GrantMinerals"));
    const TriggerTreePath elseAction{{"actions",1},{"else",0}};
    assert(editor.setTriggerTreeField(
        p,elseAction,"amount",picojson::value(20.0)));
    assert(editor.addTriggerTreeNode(p,{},"conditions","PlayerMineralsAtLeast"));
    const auto rootConditions=TriggerTree::listSnapshot(editor.triggerObject(p,0),{},"conditions");
    assert(rootConditions.size()==1);
    assert(!editor.removeTriggerTreeNode(p,{{"actions",1},{"then",0}}));
    assert(!editor.addTriggerTreeNode(p,{{"actions",0}},"then","ShowMessage"));
    assert(!editor.addTriggerTreeNode(p,branch,"conditions","ApplyEffect"));
    assert(!editor.addTriggerTreeNode(p,branch,"then","ApplyEffect"));
    assert(!editor.setTriggerTreeField(p,{{"actions",999}},"text",picojson::value("invalid")));
    assert(!editor.setTriggerExpressionNode(p,nested,"value_expr",{},
       TriggerTree::defaultExpression("Variable","unknown")));
    assert(!editor.addTriggerTreeNode(p,{},"actions","UnknownCommand"));
    (void)parseTriggerRules(p);
    p.validate();
    assert(editor.renameTriggerVariable(p,"score","mission_score"));
    const auto rule=editor.triggerObject(p,0);
    const auto& child=TriggerTree::listSnapshot(rule,loop,"actions").back().get<picojson::object>();
    const auto& expr=child.at("value_expr").get<picojson::object>();
    const auto& lhs=expr.at("left").get<picojson::object>();
    assert(lhs.at("name").get<std::string>()=="mission_score");
    assert(editor.undo(p));
    assert(parseTriggerVariables(p).count("score")==1);
    assert(editor.redo(p));
    assert(parseTriggerVariables(p).count("mission_score")==1);
    const RTSProject saved=RTSProject::fromJson(p.toJson());
    assert(parseTriggerRules(saved).size()==1);
    assert(saved.extensions.at("triggers").serialize()==
           p.extensions.at("triggers").serialize());
    std::cout<<"recursive ECA authoring: nested transactions, expressions, undo and JSON passed\n";
}
