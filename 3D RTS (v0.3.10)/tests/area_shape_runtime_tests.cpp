#include "RTSSkirmish.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Shape runtime test needs assertions in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
bool invalid(const RTSProject& project){
    try{project.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
int idAt(const RTSSkirmish& game,float x,float z){
    for(const auto& unit:game.units)
        if(unit.catalogId=="shape_dummy" &&
           std::fabs(unit.position.x-x)<.05f &&
           std::fabs(unit.position.z-z)<.05f)return unit.id;
    return -1;
}
float hp(const RTSSkirmish& game,int id){
    for(const auto& u:game.units)if(u.id==id)return u.currentHp;
    return -1.f;
}
int selectCaster(RTSSkirmish& game){
    int id=-1;
    for(const auto& u:game.units)
        if(id<0 && u.ownerId==0 && u.catalogId=="ranger")id=u.id;
    for(auto& u:game.units)u.isSelected=u.id==id;
    return id;
}
CatalogEntry area(const char* id,const char* shape,int delay){
    CatalogEntry a;a.id=id;a.type="Ability";
    a.fields={{"name",picojson::value(id)},
        {"target",picojson::value("Ground")},
        {"effect",picojson::value("shape_damage")},
        {"area_shape",picojson::value(shape)},
        {"area_radius",picojson::value(4.)},
        {"area_angle",picojson::value(60.)},
        {"area_width",picojson::value(1.5)},
        {"area_team",picojson::value("Enemy")},
        {"impact_delay_ticks",picojson::value(double(delay))},
        {"cooldown_ticks",picojson::value(1.)},
        {"cast_range",picojson::value(12.)}};
    return a;
}
RTSProject fixture(){
    auto p=RTSProject::defaults();
    p.metadata["enemy_ai_enabled"]=picojson::value(false);
    p.metadata["enemy_spawn_interval"]=picojson::value(999.);
    CatalogEntry effect;effect.id="shape_damage";effect.type="Effect";
    effect.fields={{"kind",picojson::value("Damage")},
                   {"amount",picojson::value(11.)}};
    assert(p.catalog.add(effect));
    assert(p.catalog.add(area("line_wave","Line",0)));
    assert(p.catalog.add(area("cone_wave","Cone",2)));
    for(const char* field:{"damage","attack_range","aggro_range"})
        assert(p.catalog.setField("ranger",field,picojson::value(0.)));
    assert(p.catalog.setField("ranger","max_energy",picojson::value(100.)));
    assert(p.catalog.setField("ranger","abilities",picojson::value(
        picojson::array{picojson::value("line_wave"),picojson::value("cone_wave")})));
    assert(p.catalog.cloneDerived("rift_raider","shape_dummy"));
    for(const char* field:{"damage","armor","aggro_range","move_speed"})
        assert(p.catalog.setField("shape_dummy",field,picojson::value(0.)));
    assert(p.catalog.setField("shape_dummy","max_hp",picojson::value(400.)));
    struct Target {float x,z;};
    const Target targets[]={{-6.f,-4.5f},{-7.f,-2.5f},
                            {-9.f,-4.5f},{-3.f,-4.5f}};
    for(int i=0;i<4;++i){
        ProjectPlacement place;place.kind="Unit";
        place.id="shape_target_"+std::to_string(i);
        place.name="Shape Target "+std::to_string(i);
        place.catalogId="shape_dummy";place.owner=1;
        place.position={targets[i].x,0.f,targets[i].z};
        p.placements.push_back(place);
    }
    p.validate();return p;
}
}
int main(){
    const auto project=RTSProject::fromJson(fixture().toJson());
    assert(compileAbility(project.catalog,"cone_wave").areaShape==RTSAreaShape::Cone);
    assert(compileAbility(project.catalog,"line_wave").areaShape==RTSAreaShape::Line);
    RTSSkirmish game;game.loadProject(project);
    const int caster=selectCaster(game);
    const int front=idAt(game,-6.f,-4.5f);
    const int side=idAt(game,-7.f,-2.5f);
    const int back=idAt(game,-9.f,-4.5f);
    const int outside=idAt(game,-3.f,-4.5f);
    assert(caster>0 && front>0 && side>0 && back>0 && outside>0);
    const float initial=hp(game,front);
    assert(hp(game,side)==initial && hp(game,back)==initial);
    const Vec3 impact{-8.f,0.f,-4.5f};
    assert(game.canCastAbilityAt(caster,"line_wave",impact));
    std::ofstream out("area_shapes_fingerprint.txt",
                      std::ios::binary|std::ios::trunc);
    assert(out.is_open());
    out<<"0 "<<game.stateDigest()<<"\n";
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,
                                   impact,-1,false,"line_wave"));
    game.update(1.f/60.f);
    assert(game.takeCommandOutcomes().at(0).accepted);
    assert(hp(game,front)==initial-11.f);
    assert(hp(game,side)==initial && hp(game,back)==initial &&
           hp(game,outside)==initial);
    out<<"1 "<<game.stateDigest()<<"\n";
    assert(game.queuePlayerCommand(RTSCommandKind::CastAbility,
                                   impact,-1,false,"cone_wave"));
    game.update(1.f/60.f);
    const auto watch=game.areaImpactWatch();
    assert(watch.size()==1 && watch[0].shape==RTSAreaShape::Cone &&
           watch[0].areaAngle==60.f && watch[0].remainingTicks==2);
    out<<"2 "<<game.stateDigest()<<"\n";
    game.update(1.f/60.f);
    game.update(1.f/60.f);
    assert(game.pendingAreaImpactCount()==0);
    assert(hp(game,front)==initial-22.f);
    assert(hp(game,side)==initial && hp(game,back)==initial &&
           hp(game,outside)==initial);
    out<<"4 "<<game.stateDigest()<<"\n";
    for(int i=0;i<65;++i){
        game.update(1.f/60.f);
        if(i%10==0)out<<i+5<<" "<<game.stateDigest()<<"\n";
    }
    out.flush();assert(out.good());
    const auto tape=game.replayTape();
    assert(tape.commands.size()==2 && game.replayRecordingValid());
    std::string diagnostic;
    assert(verifyRTSReplay(tape,&diagnostic));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&diagnostic));
    auto tampered=tape;tampered.commands[1].point.x+=2.f;
    assert(!verifyRTSReplay(tampered,&diagnostic));

    // Arrival samples moved targets. This direct state manipulation is
    // deliberately excluded from the portable replay fixture.
    RTSSkirmish dodge;dodge.loadProject(project);
    const int dodgeCaster=selectCaster(dodge);
    const int dodgeSide=idAt(dodge,-7.f,-2.5f);
    assert(dodge.queuePlayerCommand(RTSCommandKind::CastAbility,
                                    impact,-1,false,"cone_wave"));
    dodge.update(1.f/60.f);
    for(auto& u:dodge.units)if(u.id==dodgeSide)
        u.position={-6.f,u.position.y,-4.6f};
    for(auto& u:dodge.units)if(u.id==dodgeCaster)
        u.position={-13.f,u.position.y,-7.5f};
    dodge.update(1.f/60.f);
    dodge.update(1.f/60.f);
    assert(hp(dodge,dodgeSide)==initial-11.f);
    for(const auto& candidate:std::vector<std::pair<const char*,double>>{
        {"area_angle",0.},{"area_angle",171.},
        {"area_width",0.},{"area_width",13.}}){
        auto invalidProject=project;
        assert(invalidProject.catalog.setField("cone_wave",candidate.first,
                              picojson::value(candidate.second)));
        assert(invalid(invalidProject));
    }
    auto malformed=project;
    assert(malformed.catalog.setField("cone_wave","area_shape",
                                      picojson::value("Ellipse")));
    assert(invalid(malformed));
    std::cout<<"Shaped area runtime, impact timing, replay and dodge passed\n";
}
