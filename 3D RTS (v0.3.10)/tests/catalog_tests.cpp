#include "DataCatalog.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <utility>
int main(){
    auto c=DataCatalog::defaults();
    const auto original=unitFromCatalog(c,"ranger");
    assert(original.damage==7.f);
    assert(c.cloneDerived("ranger","super_ranger"));
    assert(c.setField("super_ranger","damage",picojson::value(23.0)));
    assert(unitFromCatalog(c,"super_ranger").damage==23.f);
    assert(unitFromCatalog(c,"ranger").damage==7.f);
    assert(c.cloneDerived("ranger_actor","stealth_actor"));
    assert(c.setField("stealth_actor","opacity",picojson::value(0.35)));
    assert(actorFromCatalog(c,"stealth_actor").opacity==.35f);
    c.validate();
    auto loaded=DataCatalog::deserialize(c.serialize());
    assert(unitFromCatalog(loaded,"super_ranger").damage==23.f);
    assert(loaded.resolvedFields("super_ranger").count("actor")==1);
    assert(loaded.setField("ranger","arbitrary_custom_field",picojson::value("preserved")));
    assert(loaded.resolvedFields("ranger").at("arbitrary_custom_field").get<std::string>()=="preserved");
    assert(!loaded.cloneDerived("missing","invalid"));
    assert(loaded.cloneDerived("ranger","cycle_a"));
    assert(loaded.cloneDerived("cycle_a","cycle_b"));
    loaded.find("cycle_a")->parent="cycle_b";
    bool cycle=false;
    try{loaded.validate();}catch(const std::invalid_argument&){cycle=true;}
    assert(cycle);
    // Catalog ID indexing must survive vector growth, reallocation,
    // copy/move, serialization and inherited-field edits at creator scale.
    DataCatalog large=DataCatalog::defaults();
    for(int i=0;i<2400;++i)
        assert(large.cloneDerived("ranger","variant_"+std::to_string(i)));
    for(int i=0;i<2400;++i) {
        const auto id="variant_"+std::to_string(i);
        assert(large.find(id) && large.find(id)->id==id);
    }
    assert(large.setField("variant_2399","damage",picojson::value(123.0)));
    assert(unitFromCatalog(large,"variant_2399").damage==123.f);
    large.validate();
    auto cloned=large;
    assert(cloned.find("variant_0") && cloned.find("variant_2399"));
    assert(cloned.setField("variant_17","damage",picojson::value(20.0)));
    assert(unitFromCatalog(large,"variant_17").damage==7.f);
    auto moved=std::move(cloned);
    assert(moved.find("variant_17") && moved.find("variant_2399"));
    auto restored=DataCatalog::deserialize(moved.serialize());
    assert(restored.find("variant_2399")!=nullptr);
    assert(unitFromCatalog(restored,"variant_2399").damage==123.f);
    // Identity changes cannot silently corrupt the index.
    restored.find("variant_0")->id="renamed_without_api";
    bool rejected=false;
    try{restored.validate();}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::cout<<"catalog: indexed lookup, 2400 derived objects, copy/move and validation passed\n";
}
