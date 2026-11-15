#pragma once
#include "RTSCatalogNavigator.hpp"
#include "RTSRequirements.hpp"
#include "RTSUpgrades.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// An authoring-only projection of real catalog references. It does NOT decide
// admission, simplify boolean gates, change research state, or hash into replays.
// Requirement graphs remain the authoritative source of Show/Use semantics.
enum class RTSTechLinkKind { Research, Train, Equip, Show, Use, Modifies };
inline const char* rtsTechLinkName(RTSTechLinkKind kind){
    switch(kind){
        case RTSTechLinkKind::Research:return "Research";
        case RTSTechLinkKind::Train:return "Train";
        case RTSTechLinkKind::Equip:return "Equip";
        case RTSTechLinkKind::Show:return "Show";
        case RTSTechLinkKind::Use:return "Use";
        case RTSTechLinkKind::Modifies:return "Modifies";
    }
    return "";
}
struct RTSTechNode {
    std::string id,type,title;
    int column=0,row=0;
    int maxLevel=0;
    int otherGates=0; // valid predicates not projected as unit/upgrade edges
    bool producer=false;
};
struct RTSTechLink {
    std::string from,to,requirementId;
    RTSTechLinkKind kind=RTSTechLinkKind::Research;
    int threshold=0; // Upgrade level or Unit count for Show/Use references
    bool negated=false; // odd number of parent Not nodes
    bool anyBranch=false; // contained in at least one Any node
};
struct RTSTechGraph {
    std::vector<RTSTechNode> nodes;
    std::vector<RTSTechLink> links;
    bool truncated=false;
    int rowCount[4]={0,0,0,0};
};

// The typed reference graph and the validated Requirement compiler already
// implement inheritance, references and nested expression semantics. Reuse
// those instead of independently parsing guessed JSON structures.
inline RTSTechGraph rtsBuildTechGraph(const RTSProject& project){
    constexpr std::size_t MaxNodes=512,MaxLinks=2048;
    RTSTechGraph out;
    std::map<std::string,RTSTechNode> nodes;
    const auto addNode=[&](const std::string& id)->bool{
        if(nodes.count(id))return true;
        const auto* e=project.catalog.find(id);
        if(!e || (e->type!="Unit" && e->type!="Ability" &&
                  e->type!="Upgrade"))return false;
        if(nodes.size()>=MaxNodes){out.truncated=true;return false;}
        RTSTechNode node;
        node.id=id;node.type=e->type;node.title=id;
        if(e->type=="Upgrade")
            node.maxLevel=compileUpgrade(project.catalog,id).maxLevel;
        const auto fields=project.catalog.resolvedFields(id);
        const auto name=fields.find("name");
        if(name!=fields.end() && name->second.is<std::string>() &&
           !name->second.get<std::string>().empty())
            node.title=name->second.get<std::string>();
        nodes.emplace(id,std::move(node));
        return true;
    };
    const auto addLink=[&](const RTSTechLink& link){
        if(out.links.size()>=MaxLinks){out.truncated=true;return;}
        if(!addNode(link.from) || !addNode(link.to))return;
        out.links.push_back(link);
    };
    // Keep orphaned Upgrade objects visible so creators can find tech that is
    // not yet reachable from a producer. Existing typed navigator edges also
    // include inherited and overridden producer/ability/unit relationships.
    for(const auto& e:project.catalog.entries())
        if(e.type=="Upgrade")addNode(e.id);
    for(const auto& edge:rtsCatalogLinks(project)){
        const auto* from=project.catalog.find(edge.from);
        if(!from || from->type!="Unit")continue;
        RTSTechLinkKind kind;
        if(edge.field=="researchable_upgrades")kind=RTSTechLinkKind::Research;
        else if(edge.field=="trainable_units")kind=RTSTechLinkKind::Train;
        else if(edge.field=="abilities")kind=RTSTechLinkKind::Equip;
        else continue;
        addLink({edge.from,edge.to,"",kind});
        if(nodes.count(edge.from))nodes[edge.from].producer=true;
    }
    // A fixed target Unit in an Upgrade definition is a modifier target,
    // not a technology gate. Display it separately from Show/Use.
    for(const auto& e:project.catalog.entries())if(e.type=="Upgrade"){
        const auto definition=compileUpgrade(project.catalog,e.id);
        if(!definition.unitId.empty())
            addLink({e.id,definition.unitId,"",RTSTechLinkKind::Modifies});
    }
    for(const auto& e:project.catalog.entries()){
        if(e.type!="Unit" && e.type!="Ability" && e.type!="Upgrade")
            continue;
        const auto fields=project.catalog.resolvedFields(e.id);
        const std::array<std::pair<const char*,RTSTechLinkKind>,2> gates{{
            {"show_requirement",RTSTechLinkKind::Show},
            {"requirement",RTSTechLinkKind::Use}
        }};
        for(const auto& gate:gates){
            const auto it=fields.find(gate.first);
            if(it==fields.end() || !it->second.is<std::string>() ||
               it->second.get<std::string>().empty())continue;
            const auto requirementId=it->second.get<std::string>();
            const auto tree=compileRequirement(project.catalog,requirementId);
            const std::function<void(const RTSRequirementDefinition&,bool,bool)>
            visit=[&](const RTSRequirementDefinition& node,bool negated,bool anyBranch){
                if(node.kind==RTSRequirementKind::Predicate){
                    const auto& p=node.predicate;
                    std::string from;
                    if(p.kind==RTSValidatorKind::UpgradeLevelAtLeast)
                        from=p.upgradeId;
                    else if(p.kind==RTSValidatorKind::OwnerUnitCountAtLeast)
                        from=p.unitId;
                    if(from.empty()){
                        if(addNode(e.id))++nodes[e.id].otherGates;
                        return;
                    }
                    addLink({from,e.id,requirementId,gate.second,
                             static_cast<int>(p.value),negated,anyBranch});
                    return;
                }
                for(const auto& child:node.children)
                    visit(child,negated^(node.kind==RTSRequirementKind::Not),
                          anyBranch || node.kind==RTSRequirementKind::Any);
            };
            visit(tree,false,false);
        }
    }
    // Stable order regardless of insertion/serialization order; bounded
    // layout is grouped by producers, upgrades, other Units and Abilities.
    for(auto& item:nodes){
        auto& n=item.second;
        n.column=n.type=="Upgrade"?1:n.type=="Ability"?3:n.producer?0:2;
        out.nodes.push_back(n);
    }
    std::sort(out.nodes.begin(),out.nodes.end(),
              [](const RTSTechNode& a,const RTSTechNode& b){
                  return std::tie(a.column,a.id)<std::tie(b.column,b.id);
              });
    for(auto& n:out.nodes)n.row=out.rowCount[n.column]++;
    std::sort(out.links.begin(),out.links.end(),
              [](const RTSTechLink& a,const RTSTechLink& b){
                  return std::tie(a.from,a.to,a.kind,a.threshold,a.negated,
                                  a.anyBranch,a.requirementId)<
                         std::tie(b.from,b.to,b.kind,b.threshold,b.negated,
                                  b.anyBranch,b.requirementId);
              });
    out.links.erase(std::unique(out.links.begin(),out.links.end(),
              [](const RTSTechLink& a,const RTSTechLink& b){
                  return std::tie(a.from,a.to,a.kind,a.threshold,a.negated,
                                  a.anyBranch,a.requirementId)==
                         std::tie(b.from,b.to,b.kind,b.threshold,b.negated,
                                  b.anyBranch,b.requirementId);
              }),out.links.end());
    return out;
}
inline std::string rtsTechDotEscape(const std::string& s){
    std::string out;out.reserve(s.size()+8);
    for(const char c:s){
        if(c=='\\' || c=='"'){out+='\\';out+=c;}
        else if(c=='\n')out+="\\n";
        else if(c=='\r')out+="\\r";
        else if(static_cast<unsigned char>(c)<32)out+=' ';
        else out+=c;
    }
    return out;
}
// Optional CLI export for external graph tooling. A visual dependency
// reference is never a guarantee that a Boolean gate is necessary.
inline std::string rtsTechGraphDot(const RTSTechGraph& graph){
    std::ostringstream out;
    out<<"digraph creator_tech {\n  rankdir=LR;\n  node [shape=box,style=rounded];\n";
    for(const auto& n:graph.nodes)
        out<<"  \""<<rtsTechDotEscape(n.id)<<"\" [label=\""
           <<rtsTechDotEscape(n.title)<<"\\n"<<n.type
           <<"\"];\n";
    for(const auto& e:graph.links){
        out<<"  \""<<rtsTechDotEscape(e.from)<<"\" -> \""
           <<rtsTechDotEscape(e.to)<<"\" [label=\""
           <<rtsTechLinkName(e.kind);
        if(e.threshold)out<<" >= "<<e.threshold;
        if(e.negated)out<<" NOT";
        if(e.anyBranch)out<<" ANY";
        out<<"\"";
        if(e.negated)out<<",style=dashed";
        out<<"];\n";
    }
    out<<"}\n";
    return out.str();
}
