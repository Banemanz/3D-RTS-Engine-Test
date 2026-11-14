#include "RTSSkirmish.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

static picojson::value effectTrigger(const std::string& id,
                                      const std::string& region) {
    picojson::object ev{{"type",picojson::value("OnRegionEnter")},
                        {"region",picojson::value(region)}};
    picojson::object act{{"type",picojson::value("ApplyEffect")},
                         {"effect",picojson::value(id)}};
    picojson::object cond{{"type",picojson::value("SubjectOwnerIs")},
                          {"owner",picojson::value(0.0)}};
    return picojson::value(picojson::object{
        {"id",picojson::value("zone_effect")},
        {"event",picojson::value(ev)},
        {"conditions",picojson::value(picojson::array{picojson::value(cond)})},
        {"actions",picojson::value(picojson::array{picojson::value(act)})},
        {"once",picojson::value(true)}
    });
}

int main() {
    RTSProject p=RTSProject::defaults();
    const auto baseHit=compileEffect(p.catalog,"kinetic_pulse");
    assert(baseHit.size()==1 && baseHit[0].kind==EffectOperationKind::Damage);
    assert(baseHit[0].amount==12.f);
    CatalogEntry combo;
    combo.id="impact_repair";
    combo.type="Effect";
    combo.fields["kind"]=picojson::value("Sequence");
    combo.fields["children"]=picojson::value(picojson::array{
        picojson::value("kinetic_pulse"),picojson::value("restorative_pulse")
    });
    assert(p.catalog.add(combo));
    const auto operations=compileEffect(p.catalog,"impact_repair");
    assert(operations.size()==2);
    assert(operations[1].kind==EffectOperationKind::Heal);
    assert(operations[1].amount==8.f);
    ProjectPlacement region;
    region.id="test_recovery_zone";
    region.kind="Region";
    region.position={-13.f,0.f,-4.5f};
    region.overrides["radius"]=picojson::value(1.8);
    p.placements.push_back(region);
    p.extensions["triggers"]=picojson::value(picojson::array{
        effectTrigger("impact_repair",region.id)
    });
    p.validate();
    TriggerEngine::validate(p);
    RTSSkirmish game;
    game.loadProject(p);
    int id=-1;
    for(const auto& u:game.units)if(u.ownerId==0 && u.catalogId=="ranger"){
        id=u.id;break;
    }
    assert(id>0);
    auto hp=[&](){for(const auto& u:game.units)
                    if(u.id==id)return u.currentHp;
                 return -1.f;};
    const float initial=hp();
    assert(initial==45.f);
    game.update(1.f/60.f);
    assert(hp()==initial-4.f);
    for(int i=0;i<20;++i)game.update(1.f/60.f);
    assert(hp()==initial-4.f); // once=true, and OnRegionEnter does not refire
    assert(game.applyEffectForTrigger("restorative_pulse",id));
    assert(hp()==initial); // healing clamps at max health
    assert(!game.applyEffectForTrigger("restorative_pulse",-1));
    assert(!p.catalogReferences("kinetic_pulse").empty());
    std::string reason;
    assert(!p.removeCatalogObject("kinetic_pulse",&reason));
    assert(reason.find("children")!=std::string::npos);
    assert(p.renameCatalogObject("kinetic_pulse","impact_pulse"));
    const auto sequence=p.catalog.resolvedFields("impact_repair").at("children")
                             .get<picojson::array>();
    assert(sequence[0].get<std::string>()=="impact_pulse");
    assert(p.renameCatalogObject("impact_repair","effect_v2"));
    TriggerEngine::validate(p);
    assert(!p.catalogReferences("effect_v2").empty());
    assert(!p.removeCatalogObject("effect_v2",&reason));

    RTSProject bad=p;
    CatalogEntry cycle;
    cycle.id="recursive_fx";
    cycle.type="Effect";
    cycle.fields["kind"]=picojson::value("Sequence");
    cycle.fields["children"]=picojson::value(picojson::array{
        picojson::value("recursive_fx")
    });
    assert(bad.catalog.add(cycle));
    bool rejected=false;
    try{bad.validate();}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    bad=p;
    CatalogEntry negative;
    negative.id="negative_fx";
    negative.type="Effect";
    negative.fields["kind"]=picojson::value("Damage");
    negative.fields["amount"]=picojson::value(-1.0);
    assert(bad.catalog.add(negative));
    rejected=false;
    try{bad.validate();}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    bad=p;
    auto& rules=bad.extensions.at("triggers").get<picojson::array>();
    rules[0].get<picojson::object>().at("event")=picojson::value(picojson::object{
        {"type",picojson::value("OnMapStart")}
    });
    rejected=false;
    try{TriggerEngine::validate(bad);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);

    std::cout<<"effects: typed graphs, region execution, bounds, cycles and references passed\\n";
}
