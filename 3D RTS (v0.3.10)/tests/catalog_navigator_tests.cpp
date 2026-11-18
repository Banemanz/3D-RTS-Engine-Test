#include "RTSCatalogNavigator.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Catalog Navigator requires active Release assertions"
#endif
#include <iostream>
#include <string>
#include <vector>
namespace {
bool has(const std::vector<RTSCatalogLink>& links,
         const char* from,const char* field,const char* to,bool inherited=false){
    for(const auto& link:links)if(link.from==from && link.to==to &&
        link.field==field && link.inherited==inherited)return true;
    return false;
}
CatalogEntry object(const char* type,const char* id){
    CatalogEntry e;e.type=type;e.id=id;return e;
}
}
int main(){
    RTSProject project=RTSProject::defaults();
    const auto& baseline=project.catalog.entries();
    assert(!baseline.empty());
    auto projectile=object("Projectile","nav_bolt");
    projectile.fields={{"speed",picojson::value(10.)}};
    assert(project.catalog.add(projectile));
    auto weapon=object("Weapon","nav_rifle");
    weapon.fields={{"projectile",picojson::value("nav_bolt")}};
    assert(project.catalog.add(weapon));
    assert(project.catalog.setField("ranger","weapon",picojson::value("nav_rifle")));
    assert(project.catalog.cloneDerived("ranger","nav_ranger"));
    project.validate();
    const auto ranger=rtsCatalogLinks(project,"ranger");
    assert(has(ranger,"ranger","weapon","nav_rifle"));
    assert(has(ranger,"nav_ranger","parent","ranger"));
    const auto weaponEdges=rtsCatalogLinks(project,"nav_rifle");
    assert(has(weaponEdges,"ranger","weapon","nav_rifle"));
    assert(has(weaponEdges,"nav_ranger","weapon","nav_rifle",true));
    assert(has(weaponEdges,"nav_rifle","projectile","nav_bolt"));
    const auto boltEdges=rtsCatalogLinks(project,"nav_bolt");
    assert(has(boltEdges,"nav_rifle","projectile","nav_bolt"));
    // An explicit empty override must remove the inherited weapon edge.
    assert(project.catalog.setField("nav_ranger","weapon",picojson::value("")));
    project.validate();
    const auto overridden=rtsCatalogLinks(project,"nav_rifle");
    assert(!has(overridden,"nav_ranger","weapon","nav_rifle",true));
    assert(has(overridden,"ranger","weapon","nav_rifle"));
    // Typed graph is not a free-text search; arbitrary mod extension values
    // cannot be mistaken for a link.
    assert(project.catalog.setField("ranger","custom_mod_note",
                                    picojson::value("nav_bolt")));
    project.validate();
    for(const auto& edge:rtsCatalogLinks(project))
        assert(edge.field!="custom_mod_note");
    const auto all=rtsCatalogLinks(project);
    for(std::size_t i=1;i<all.size();++i)
        assert(all[i-1].from<all[i].from ||
               (all[i-1].from==all[i].from &&
                (all[i-1].field<all[i].field ||
                 (all[i-1].field==all[i].field &&
                  all[i-1].to<=all[i].to))));
    std::cout<<"Typed forward/reverse graph, inheritance and stable sorting passed\n";
}
