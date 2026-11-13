#include "DataCatalog.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
int main(){
    auto c=DataCatalog::defaults();
    const auto original=unitFromCatalog(c,"marine");
    assert(original.damage==7.f);
    assert(c.cloneDerived("marine","super_marine"));
    assert(c.setField("super_marine","damage",picojson::value(23.0)));
    assert(unitFromCatalog(c,"super_marine").damage==23.f);
    assert(unitFromCatalog(c,"marine").damage==7.f);
    assert(c.cloneDerived("marine_actor","stealth_actor"));
    assert(c.setField("stealth_actor","opacity",picojson::value(0.35)));
    assert(actorFromCatalog(c,"stealth_actor").opacity==.35f);
    c.validate();
    auto loaded=DataCatalog::deserialize(c.serialize());
    assert(unitFromCatalog(loaded,"super_marine").damage==23.f);
    assert(loaded.resolvedFields("super_marine").count("actor")==1);
    assert(loaded.setField("marine","arbitrary_custom_field",picojson::value("preserved")));
    assert(loaded.resolvedFields("marine").at("arbitrary_custom_field").get<std::string>()=="preserved");
    assert(!loaded.cloneDerived("missing","invalid"));
    assert(loaded.cloneDerived("marine","cycle_a"));
    assert(loaded.cloneDerived("cycle_a","cycle_b"));
    loaded.find("cycle_a")->parent="cycle_b";
    bool cycle=false;
    try{loaded.validate();}catch(const std::invalid_argument&){cycle=true;}
    assert(cycle);
    std::cout<<"catalog: typed inheritance, validation, visual refs and JSON persistence passed\n";
}
