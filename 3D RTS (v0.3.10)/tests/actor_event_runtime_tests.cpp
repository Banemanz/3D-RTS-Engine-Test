#include "RTSSkirmish.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Actor cue runtime checks must be active in Release"
#endif
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
picojson::value cue(const char* event,const char* style,float radius,
                    int duration,float r=.2f,float g=.8f,float b=1.f){
    return picojson::value(picojson::object{
        {"when",picojson::value(event)},
        {"style",picojson::value(style)},
        {"radius",picojson::value(double(radius))},
        {"duration_ticks",picojson::value(double(duration))},
        {"tint",picojson::value(picojson::array{
            picojson::value(double(r)),picojson::value(double(g)),
            picojson::value(double(b))})}});
}
RTSProject fixture(){
    auto project=RTSProject::defaults();
    project.metadata["enemy_ai_enabled"]=picojson::value(false);
    project.metadata["enemy_spawn_interval"]=picojson::value(999.);
    CatalogEntry projectile;projectile.type="Projectile";projectile.id="cue_bolt";
    projectile.fields={{"speed",picojson::value(6.)},
                       {"arc_height",picojson::value(0.)},
                       {"collision_radius",picojson::value(.4)},
                       {"collide_units",picojson::value(false)}};
    assert(project.catalog.add(projectile));
    CatalogEntry weapon;weapon.type="Weapon";weapon.id="cue_rifle";
    weapon.fields={{"damage",picojson::value(20.)},
                   {"range",picojson::value(12.)},
                   {"cooldown",picojson::value(10.)},
                   {"armor_piercing",picojson::value(3.)},
                   {"projectile",picojson::value("cue_bolt")}};
    assert(project.catalog.add(weapon));
    assert(project.catalog.setField("ranger","weapon",picojson::value("cue_rifle")));
    assert(project.catalog.setField("ranger","aggro_range",picojson::value(0.)));
    assert(project.catalog.setField("ranger_actor","events",picojson::value(
        picojson::array{cue("Attack","Pulse",1.5f,90),
                         cue("Launch","Beacon",1.4f,90),
                         cue("Impact","Ring",2.f,90),
                         cue("Cast","Pulse",2.f,90)})));
    assert(project.catalog.setField("rift_raider_actor","events",
        picojson::value(picojson::array{
            cue("Death","Beacon",2.f,60,1.f,.2f,.2f),
            cue("Status","Pulse",1.1f,30,.3f,1.f,.5f)})));
    assert(project.catalog.cloneDerived("rift_raider","cue_dummy"));
    for(const char* key:{"damage","attack_range","aggro_range","move_speed"})
        assert(project.catalog.setField("cue_dummy",key,picojson::value(0.)));
    assert(project.catalog.setField("cue_dummy","armor",picojson::value(8.)));
    assert(project.catalog.setField("cue_dummy","max_hp",picojson::value(500.)));
    ProjectPlacement placed;placed.id="cue_target";placed.kind="Unit";
    placed.catalogId="cue_dummy";placed.name="Cue Target";placed.owner=1;
    placed.position={-8.f,0.f,-4.5f};
    project.placements.push_back(placed);
    project.validate();
    return project;
}
int findId(const RTSSkirmish& game,const char* name){
    for(const auto& u:game.units)if(u.instanceName==name)return u.id;
    return -1;
}
bool has(const RTSSkirmish& game,RTSActorCueEvent event,int id){
    for(const auto& item:game.actorCueWatch())
        if(item.unitId==id && item.event==event)return true;
    return false;
}
bool invalid(const RTSProject& p){
    try{p.validate();}catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(){
    const RTSProject project=fixture();
    const auto cues=compileActorCues(project.catalog,"ranger_actor");
    assert(cues.size()==4 && cues[0].event==RTSActorCueEvent::Attack);
    assert(cues[1].style==RTSActorCueStyle::Beacon);
    assert(cues[0].durationTicks==90);
    RTSSkirmish game;game.loadProject(RTSProject::fromJson(project.toJson()));
    int shooter=-1;
    for(const auto& u:game.units)
        if(shooter<0 && u.ownerId==0 && u.catalogId=="ranger")shooter=u.id;
    const int target=findId(game,"Cue Target");
    assert(shooter>0 && target>0);
    for(auto& u:game.units)u.isSelected=u.id==shooter;
    assert(game.actorCueWatch().empty());
    std::ofstream fingerprint("actor_cues_fingerprint.txt",
                              std::ios::binary|std::ios::trunc);
    assert(fingerprint.is_open());
    fingerprint<<"0 "<<game.stateDigest()<<"\n";
    assert(game.queuePlayerCommand(RTSCommandKind::Attack,{},target));
    game.update(1.f/60.f);game.update(1.f/60.f);
    assert(game.pendingWeaponShotCount()==1);
    assert(has(game,RTSActorCueEvent::Attack,shooter));
    assert(has(game,RTSActorCueEvent::Launch,shooter));
    assert(!has(game,RTSActorCueEvent::Impact,shooter));
    const auto digest=game.stateDigest();
    const auto watch=game.actorCueWatch(1);
    assert(watch.size()==1 && game.stateDigest()==digest);
    assert(game.actorCueWatch().size()>=2);
    fingerprint<<"2 "<<game.stateDigest()<<"\n";
    for(int i=3;i<=75;++i){
        game.update(1.f/60.f);
        if(i%15==0)fingerprint<<i<<" "<<game.stateDigest()<<"\n";
    }
    assert(game.pendingWeaponShotCount()==0);
    assert(has(game,RTSActorCueEvent::Impact,shooter));
    fingerprint.flush();assert(fingerprint.good());
    auto tape=game.replayTape();std::string why;
    assert(tape.commands.size()==1);
    assert(verifyRTSReplay(tape,&why));
    assert(verifyRTSReplay(RTSReplayTape::fromJson(tape.toJson()),&why));
    auto tampered=tape;tampered.commands[0].targetId=shooter;
    assert(!verifyRTSReplay(tampered,&why));

    // Simulation and replay MUST be bit-for-bit identical with the same
    // gameplay definitions but every Actor event cue removed. This verifies
    // presentation state never silently enters authoritative game hashes.
    RTSProject silent=project;
    assert(silent.catalog.setField("ranger_actor","events",
                                   picojson::value(picojson::array{})));
    assert(silent.catalog.setField("rift_raider_actor","events",
                                   picojson::value(picojson::array{})));
    silent.validate();
    RTSSkirmish vividGame,quietGame;
    vividGame.loadProject(project);
    quietGame.loadProject(silent);
    int vividShooter=-1,quietShooter=-1;
    for(const auto& u:vividGame.units)if(vividShooter<0 &&
         u.ownerId==0 && u.catalogId=="ranger")vividShooter=u.id;
    for(const auto& u:quietGame.units)if(quietShooter<0 &&
         u.ownerId==0 && u.catalogId=="ranger")quietShooter=u.id;
    const int vividTarget=findId(vividGame,"Cue Target");
    const int quietTarget=findId(quietGame,"Cue Target");
    assert(vividShooter==quietShooter && vividTarget==quietTarget);
    for(auto& u:vividGame.units)u.isSelected=u.id==vividShooter;
    for(auto& u:quietGame.units)u.isSelected=u.id==quietShooter;
    assert(vividGame.stateDigest()==quietGame.stateDigest());
    assert(vividGame.queuePlayerCommand(RTSCommandKind::Attack,{},vividTarget));
    assert(quietGame.queuePlayerCommand(RTSCommandKind::Attack,{},quietTarget));
    for(int tick=0;tick<110;++tick){
        vividGame.update(1.f/60.f);
        quietGame.update(1.f/60.f);
        assert(vividGame.stateDigest()==quietGame.stateDigest());
    }
    assert(!vividGame.actorCueWatch().empty() ||
           vividGame.activeActorCueCount()==0);
    assert(quietGame.actorCueWatch().empty());

    // Self-targeted Abilities produce both Cast and Impact presentations;
    // they use precisely the same authoritative Effect pipeline.
    RTSProject castProject=project;
    CatalogEntry effect;effect.type="Effect";effect.id="cue_heal";
    effect.fields={{"kind",picojson::value("Heal")},
                   {"amount",picojson::value(5.)}};
    assert(castProject.catalog.add(effect));
    CatalogEntry ability;ability.type="Ability";ability.id="cue_cast";
    ability.fields={{"effect",picojson::value("cue_heal")},
                    {"target",picojson::value("Self")}};
    assert(castProject.catalog.add(ability));
    assert(castProject.catalog.setField("ranger","abilities",
        picojson::value(picojson::array{picojson::value("cue_cast")})));
    castProject.validate();
    RTSSkirmish castGame;castGame.loadProject(castProject);
    int castShooter=-1;
    for(const auto& u:castGame.units)if(castShooter<0 &&
         u.ownerId==0 && u.catalogId=="ranger")castShooter=u.id;
    assert(castShooter>0);
    for(auto& u:castGame.units)u.isSelected=u.id==castShooter;
    assert(castGame.queuePlayerCommand(RTSCommandKind::CastAbility,{},-1,
                                       false,"cue_cast"));
    castGame.update(1.f/60.f);
    assert(has(castGame,RTSActorCueEvent::Cast,castShooter));
    assert(has(castGame,RTSActorCueEvent::Impact,castShooter));

    // Actor events may be overridden on a single placement without changing
    // any shared Actor definition or its authoritative unit's attack data.
    RTSProject overrideProject=project;
    for(auto& placement:overrideProject.placements)if(placement.id=="cue_target"){
        placement.overrides["actor"]=picojson::value(picojson::object{
            {"events",picojson::value(picojson::array{
                cue("Death","Ring",2.5f,45,1.f,.3f,.15f)})}});
    }
    overrideProject.validate();
    RTSSkirmish dead;dead.loadProject(overrideProject);
    const int victim=findId(dead,"Cue Target");
    assert(victim>0);
    bool changed=false;
    for(auto& u:dead.units)if(u.id==victim){
        assert(u.actorCues.size()==1 &&
               u.actorCues[0].style==RTSActorCueStyle::Ring);
        u.takeDamage(2000.f,"test");
        changed=true;
    }
    assert(changed);
    dead.update(1.f/60.f);
    assert(has(dead,RTSActorCueEvent::Death,victim));
    assert(dead.actorCueWatch().size()==1);
    for(int i=0;i<46;++i)dead.update(1.f/60.f);
    assert(!has(dead,RTSActorCueEvent::Death,victim));
    // Legacy Actor catalogs without an event array have zero presentation
    // cues, and remain fully supported.
    RTSProject legacy=RTSProject::defaults();legacy.validate();
    assert(compileActorCues(legacy.catalog,"ranger_actor").empty());
    RTSSkirmish old;old.loadProject(legacy);
    assert(old.actorCueWatch().empty());

    for(const picojson::value& bad:std::vector<picojson::value>{
        picojson::value("not-an-array"),
        picojson::value(picojson::array{cue("Unknown","Pulse",1.f,25)}),
        picojson::value(picojson::array{cue("Attack","Unknown",1.f,25)}),
        picojson::value(picojson::array{cue("Attack","Pulse",0.f,25)}),
        picojson::value(picojson::array{cue("Attack","Pulse",1.f,181)}),
        picojson::value(picojson::array{cue("Attack","Pulse",1.f,25,2.f,0.f,0.f)}),
    }){
        auto malformed=project;
        assert(malformed.catalog.setField("ranger_actor","events",bad));
        assert(invalid(malformed));
    }
    auto tooMany=project;
    picojson::array thirteen;
    for(int i=0;i<13;++i)thirteen.push_back(cue("Attack","Ring",1.f,12));
    assert(tooMany.catalog.setField("ranger_actor","events",
                                    picojson::value(thirteen)));
    assert(invalid(tooMany));
    std::cout<<"Actor cue compile, project overrides, real attack/impact/death, TTL and replay safety passed\n";
}
