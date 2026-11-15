#include "RTSTechGraph.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#if defined(NDEBUG)
#error "Technology graph tests must keep assertions in Release"
#endif

namespace {
bool has(const RTSTechGraph& g,const char* from,const char* to,
         RTSTechLinkKind kind,int minimum=0,bool negated=false,
         bool any=false){
    for(const auto& e:g.links)
        if(e.from==from && e.to==to && e.kind==kind &&
           e.threshold==minimum && e.negated==negated &&
           e.anyBranch==any)return true;
    return false;
}
const RTSTechNode* find(const RTSTechGraph& g,const char* id){
    for(const auto& n:g.nodes)if(n.id==id)return &n;
    return nullptr;
}
}
int main(int argc,char** argv){
    if(argc!=2)throw std::invalid_argument("Expected Technology Forge sample path");
    RTSProject project;project.loadFile(argv[1]);
    project.validate();
    const std::string original=project.toJson().serialize();
    const auto g=rtsBuildTechGraph(project);
    assert(!g.truncated);
    assert(find(g,"command_center") && find(g,"frontier_tactics"));
    assert(find(g,"layered_plating") && find(g,"worker"));
    assert(find(g,"arc_lance") && find(g,"ranger"));
    assert(find(g,"command_center")->producer);
    assert(find(g,"command_center")->column==0);
    assert(find(g,"frontier_tactics")->column==1);
    assert(find(g,"worker")->column==2);
    assert(find(g,"arc_lance")->column==3);
    assert(find(g,"frontier_tactics")->maxLevel==3);
    assert(has(g,"command_center","frontier_tactics",RTSTechLinkKind::Research));
    assert(has(g,"command_center","layered_plating",RTSTechLinkKind::Research));
    assert(has(g,"frontier_tactics","ranger",RTSTechLinkKind::Modifies));
    assert(has(g,"layered_plating","worker",RTSTechLinkKind::Modifies));
    // A Show edge is a reference, not a claim that an arbitrary nested
    // Requirement simplifies to a single mandatory prerequisite.
    assert(has(g,"frontier_tactics","worker",RTSTechLinkKind::Show,1));
    assert(has(g,"frontier_tactics","worker",RTSTechLinkKind::Use,2));
    assert(has(g,"frontier_tactics","layered_plating",RTSTechLinkKind::Show,1));
    assert(has(g,"frontier_tactics","layered_plating",RTSTechLinkKind::Use,2));
    assert(has(g,"frontier_tactics","arc_lance",RTSTechLinkKind::Show,1));
    assert(has(g,"frontier_tactics","frontier_tactics",
               RTSTechLinkKind::Show,3,true));
    assert(find(g,"arc_lance")->otherGates>0);
    assert(project.toJson().serialize()==original); // pure projection
    for(std::size_t i=1;i<g.nodes.size();++i)
        assert(g.nodes[i-1].column<g.nodes[i].column ||
              (g.nodes[i-1].column==g.nodes[i].column &&
               g.nodes[i-1].id<g.nodes[i].id));
    for(int col=0;col<4;++col){
        int previous=-1;
        for(const auto& n:g.nodes)if(n.column==col){
            assert(n.row==previous+1);previous=n.row;
        }
        assert(previous+1==g.rowCount[col]);
    }
    assert(rtsTechGraphDot(g).find("frontier_tactics")!=std::string::npos);

    // Nested Any/Not provenance must survive the projection, without
    // guessing whether those links are mandatory prerequisites.
    RTSProject nested=project;
    CatalogEntry v;v.id="graph_count";v.type="Validator";
    v.fields={{"kind",picojson::value("OwnerUnitCountAtLeast")},
              {"subject",picojson::value("Caster")},
              {"unit",picojson::value("ranger")},
              {"value",picojson::value(2.)}};
    assert(nested.catalog.add(v));
    CatalogEntry pred;pred.id="graph_count_req";pred.type="Requirement";
    pred.fields={{"kind",picojson::value("Predicate")},
                 {"validator",picojson::value("graph_count")}};
    assert(nested.catalog.add(pred));
    CatalogEntry disj;disj.id="graph_any";disj.type="Requirement";
    disj.fields={{"kind",picojson::value("Any")},
                 {"children",picojson::value(picojson::array{
                     picojson::value("graph_count_req"),
                     picojson::value("r_has_tactics")})}};
    assert(nested.catalog.add(disj));
    CatalogEntry inversion;inversion.id="graph_not";inversion.type="Requirement";
    inversion.fields={{"kind",picojson::value("Not")},
                      {"children",picojson::value(picojson::array{
                          picojson::value("graph_any")})}};
    assert(nested.catalog.add(inversion));
    assert(nested.catalog.setField("worker","show_requirement",
                                   picojson::value("graph_not")));
    nested.validate();
    const auto ng=rtsBuildTechGraph(nested);
    assert(has(ng,"ranger","worker",RTSTechLinkKind::Show,2,true,true));
    assert(has(ng,"frontier_tactics","worker",RTSTechLinkKind::Show,1,true,true));
    assert(has(ng,"frontier_tactics","worker",RTSTechLinkKind::Use,2));
    assert(!has(ng,"frontier_tactics","worker",RTSTechLinkKind::Show,1));

    // Stable graph/DOT output after serialization, and no copied content
    // from an arbitrary unknown extension field.
    RTSProject round=RTSProject::fromJson(nested.toJson());
    const auto roundGraph=rtsBuildTechGraph(round);
    assert(rtsTechGraphDot(ng)==rtsTechGraphDot(roundGraph));
    assert(round.catalog.setField("worker","custom_mod_note",
                                  picojson::value("fictitious_tech_reference")));
    const auto still=rtsBuildTechGraph(round);
    assert(!find(still,"fictitious_tech_reference"));
    // DOT labels are escaped, never interpreted as graph syntax.
    assert(round.catalog.setField("worker","name",
        picojson::value(std::string("Worker \"Prototype\"\nwith label"))));
    const auto escaped=rtsTechGraphDot(rtsBuildTechGraph(round));
    assert(escaped.find("Worker \\\"Prototype\\\"\\nwith label")!=std::string::npos);
    std::cout<<"Technology dependency graph, Show/Use/NOT/ANY, inheritance, "
                "determinism, nonmutation and DOT escaping passed\n";
}
