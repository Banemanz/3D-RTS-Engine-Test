#include "RTSSkirmish.hpp"
#include "TriggerDefinition.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Timed gameplay-effect CTests require active assertions in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>

namespace {
CatalogEntry effect(const char* name,const char* kind,double amount,
                    double interval,double duration){
    CatalogEntry e;
    e.id=name;e.type="Effect";
    e.fields={{"kind",picojson::value(kind)},
              {"amount",picojson::value(amount)},
              {"interval_ticks",picojson::value(interval)},
              {"duration_ticks",picojson::value(duration)}};
    return e;
}
bool fails(const RTSProject& project){
    try{project.validate();}
    catch(const std::invalid_argument&){return true;}
    return false;
}
float health(const RTSSkirmish& game,int unitId){
    for(const auto& unit:game.units)
        if(unit.id==unitId)return unit.currentHp;
    return -1.f;
}
void near(float actual,float expected){
    assert(std::fabs(actual-expected)<0.02f);
}
}
int main(){
    RTSProject project=RTSProject::defaults();
    project.metadata["enemy_ai_enabled"]=picojson::value(false);
    project.metadata["enemy_spawn_interval"]=picojson::value(999.0);
    assert(project.catalog.add(effect("pulse_dmg","PeriodicDamage",6.0,2.0,6.0)));
    assert(project.catalog.add(effect("pulse_heal","PeriodicHeal",2.0,2.0,6.0)));
    CatalogEntry sequence;
    sequence.id="pulse_combo";sequence.type="Effect";
    sequence.fields={{"kind",picojson::value("Sequence")},
        {"children",picojson::value(picojson::array{
            picojson::value("pulse_dmg"),picojson::value("pulse_heal")
        })}};
    assert(project.catalog.add(sequence));
    const auto ops=compileEffect(project.catalog,"pulse_combo");
    assert(ops.size()==2);
    assert(ops[0].kind==EffectOperationKind::PeriodicDamage);
    assert(ops[1].kind==EffectOperationKind::PeriodicHeal);
    assert(ops[0].intervalTicks==2 && ops[0].durationTicks==6);

    // Real catalog-authored passive regeneration on only the first ranger.
    assert(project.catalog.cloneDerived("ranger","field_medic"));
    assert(project.catalog.setField("field_medic","hp_regen",picojson::value(60.0)));
    project.placements[2].catalogId="field_medic";
    ProjectPlacement region;
    region.id="pulse_testing_field";
    region.kind="Region";
    region.position={-13.f,0.f,-4.5f};
    region.overrides["radius"]=picojson::value(1.0);
    project.placements.push_back(region);
    project.extensions["triggers"]=picojson::value(picojson::array{
        picojson::value(picojson::object{
            {"id",picojson::value("pulse_region")},
            {"event",picojson::value(picojson::object{
                {"type",picojson::value("OnRegionEnter")},
                {"region",picojson::value("pulse_testing_field")}
            })},
            {"once",picojson::value(true)},
            {"conditions",picojson::value(picojson::array{
                picojson::value(picojson::object{
                    {"type",picojson::value("SubjectOwnerIs")},
                    {"owner",picojson::value(0.0)}
                })
            })},
            {"actions",picojson::value(picojson::array{
                picojson::value(picojson::object{
                    {"type",picojson::value("ApplyEffect")},
                    {"effect",picojson::value("pulse_combo")}
                })
            })}
        })
    });
    project.validate();TriggerEngine::validate(project);
    const auto roundTrip=RTSProject::fromJson(project.toJson());
    assert(compileEffect(roundTrip.catalog,"pulse_combo").size()==2);

    RTSSkirmish game;
    game.loadProject(project);
    int subject=-1;
    for(const auto& u:game.units)
        if(u.catalogId=="field_medic"){subject=u.id;break;}
    assert(subject>0);
    near(health(game,subject),45.f);
    assert(game.activePeriodicEffectCount()==0);
    std::ofstream fingerprint("timed_effect_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<game.stateDigest()<<"\n";
    for(int tick=1;tick<=120;++tick){
        game.update(1.f/60.f);
        if(tick==1){
            assert(game.activePeriodicEffectCount()==2);
            near(health(game,subject),45.f);
        }
        if(tick==3)near(health(game,subject),41.f);
        if(tick==5)near(health(game,subject),39.f);
        if(tick==7){
            near(health(game,subject),37.f);
            assert(game.activePeriodicEffectCount()==0);
        }
        if(tick==8)near(health(game,subject),38.f);
        if(tick==20)near(health(game,subject),45.f);
        if(tick%30==0)fingerprint<<tick<<" "<<game.stateDigest()<<"\n";
    }
    fingerprint.flush();assert(fingerprint.good());
    assert(game.triggerDiagnostics().rules()[0].executions==1);
    assert(game.activePeriodicEffectCount()==0);
    assert(verifyRTSReplay(game.replayTape()));

    // The timed scheduler is bounded and performs admission before applying
    // any immediate graph operations, never dropping only half a sequence.
    for(int i=0;i<128;++i)
        assert(game.applyEffectForTrigger("pulse_combo",subject));
    assert(game.activePeriodicEffectCount()==256);
    assert(!game.applyEffectForTrigger("pulse_combo",subject));
    game.reset();
    assert(game.activePeriodicEffectCount()==0);

    RTSProject broken=project;
    assert(broken.catalog.setField("pulse_dmg","interval_ticks",picojson::value(0.0)));
    assert(fails(broken));
    broken=project;
    assert(broken.catalog.setField("pulse_heal","duration_ticks",picojson::value(3601.0)));
    assert(fails(broken));
    broken=project;
    assert(broken.catalog.setField("pulse_dmg","interval_ticks",picojson::value(1.5)));
    assert(fails(broken));
    broken=project;
    assert(broken.catalog.setField("field_medic","hp_regen",picojson::value(-1.0)));
    assert(fails(broken));
    std::cout<<"timed effects: periodic pulse scheduler, regeneration, bounds, replay passed\n";
}
