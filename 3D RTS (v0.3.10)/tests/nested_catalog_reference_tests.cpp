#include "RTSProject.hpp"
#include "TriggerDefinition.hpp"
#include <cassert>
#include <iostream>
#include <string>

namespace {
picojson::value node(const char* type,picojson::object fields={}){
    fields["type"]=picojson::value(type);
    return picojson::value(fields);
}
}
int main(){
    RTSProject p=RTSProject::defaults();
    assert(p.catalog.cloneDerived("ranger","scripted_ranger"));
    assert(p.catalog.cloneDerived("kinetic_pulse","scripted_effect"));
    ProjectPlacement region;
    region.id="script_test_region";
    region.kind="Region";
    region.position={0.f,0.f,0.f};
    p.placements.push_back(region);

    p.extensions["triggers"]=picojson::value(picojson::array{
        picojson::value(picojson::object{
            {"id",picojson::value("nested_spawn")},
            {"event",picojson::value(picojson::object{
                {"type",picojson::value("OnMapStart")}
            })},
            {"actions",picojson::value(picojson::array{
                node("IfThenElse",{
                    {"conditions",picojson::value(picojson::array{
                        node("UnitTypeCountAtLeast",{
                            {"catalog",picojson::value("scripted_ranger")},
                            {"count",picojson::value(0.0)}
                        })
                    })},
                    {"then",picojson::value(picojson::array{
                        node("Repeat",{
                            {"times",picojson::value(2.0)},
                            {"actions",picojson::value(picojson::array{
                                node("SpawnUnit",{
                                    {"catalog",picojson::value("scripted_ranger")},
                                    {"owner",picojson::value(0.0)},
                                    {"x",picojson::value(0.0)},
                                    {"z",picojson::value(0.0)}
                                })
                            })}
                        })
                    })},
                    {"else",picojson::value(picojson::array{
                        node("ShowMessage",{
                            {"text",picojson::value("scripted_ranger")}
                        })
                    })}
                })
            })}
        }),
        picojson::value(picojson::object{
            {"id",picojson::value("nested_effect")},
            {"event",picojson::value(picojson::object{
                {"type",picojson::value("OnRegionEnter")},
                {"region",picojson::value("script_test_region")}
            })},
            {"actions",picojson::value(picojson::array{
                node("IfThenElse",{
                    {"conditions",picojson::value(picojson::array{
                        node("PlayerMineralsAtLeast",{
                            {"amount",picojson::value(0.0)}
                        })
                    })},
                    {"then",picojson::value(picojson::array{
                        node("ApplyEffect",{
                            {"effect",picojson::value("scripted_effect")}
                        })
                    })},
                    {"else",picojson::value(picojson::array{
                        node("ShowMessage",{
                            {"text",picojson::value("scripted_effect")}
                        })
                    })}
                })
            })}
        })
    });
    assert(parseTriggerRules(p).size()==2);
    const auto unitUses=p.catalogReferences("scripted_ranger");
    const auto effectUses=p.catalogReferences("scripted_effect");
    assert(unitUses.size()>=2);
    assert(!effectUses.empty());
    assert(unitUses[0].find("trigger")!=std::string::npos);
    std::string error;
    assert(!p.removeCatalogObject("scripted_ranger",&error));
    assert(error.find("trigger")!=std::string::npos);
    assert(!p.removeCatalogObject("scripted_effect",&error));
    assert(error.find("trigger")!=std::string::npos);

    assert(p.renameCatalogObject("scripted_ranger","archer_variant"));
    assert(p.renameCatalogObject("scripted_effect","kinetic_variant"));
    const auto rules=parseTriggerRules(p);
    assert(rules.size()==2);
    const auto& branch=rules[0].actions[0];
    const auto& cond=branch.at("conditions").get<picojson::array>()[0]
        .get<picojson::object>();
    assert(cond.at("catalog").get<std::string>()=="archer_variant");
    const auto& repeat=branch.at("then").get<picojson::array>()[0]
        .get<picojson::object>();
    const auto& spawn=repeat.at("actions").get<picojson::array>()[0]
        .get<picojson::object>();
    assert(spawn.at("catalog").get<std::string>()=="archer_variant");
    const auto& fallback=branch.at("else").get<picojson::array>()[0]
        .get<picojson::object>();
    assert(fallback.at("text").get<std::string>()=="scripted_ranger");
    const auto& effect=rules[1].actions[0].at("then").get<picojson::array>()[0]
        .get<picojson::object>();
    assert(effect.at("effect").get<std::string>()=="kinetic_variant");
    assert(!p.catalogReferences("kinetic_variant").empty());
    assert(!p.catalogReferences("archer_variant").empty());
    const auto roundtrip=RTSProject::fromJson(p.toJson());
    assert(parseTriggerRules(roundtrip).size()==2);
    p.extensions.erase("triggers");
    assert(p.removeCatalogObject("archer_variant",&error));
    assert(p.removeCatalogObject("kinetic_variant",&error));
    std::cout<<"nested catalog references: typed rename/delete guard and serialization passed\n";
}
