#include "RTSSkirmish.hpp"
#include <cassert>
#include <iostream>
static picojson::value makeTrigger(const std::string& id,const picojson::object& ev,
                                   const picojson::array& actions,bool once=true) {
    return picojson::value(picojson::object{
        {"id",picojson::value(id)},{"event",picojson::value(ev)},
        {"actions",picojson::value(actions)},{"once",picojson::value(once)}
    });
}
int main(){
    RTSProject p=RTSProject::defaults();
    picojson::array events{
      makeTrigger("start_bonus",{{"type",picojson::value("OnMapStart")}},
          {picojson::value(picojson::object{{"type",picojson::value("GrantMinerals")},
                                           {"amount",picojson::value(30.0)}})}),
      makeTrigger("later_bonus",{{"type",picojson::value("After")},
                                   {"seconds",picojson::value(1.0)}},
          {picojson::value(picojson::object{{"type",picojson::value("GrantMinerals")},
                                           {"amount",picojson::value(10.0)}})}),
      makeTrigger("spawn",{{"type",picojson::value("After")},
                             {"seconds",picojson::value(2.0)}},
          {picojson::value(picojson::object{{"type",picojson::value("SpawnUnit")},
                                           {"catalog",picojson::value("marine")},
                                           {"owner",picojson::value(0.0)},
                                           {"x",picojson::value(-6.0)},
                                           {"z",picojson::value(6.0)}})})
    };
    p.extensions["triggers"]=picojson::value(events);
    TriggerEngine::validate(p);
    RTSSkirmish game;
    game.loadProject(p);
    assert(game.playerMinerals==250);
    for(int i=0;i<150;++i)game.update(1.0f/60.0f);
    assert(game.playerMinerals==260);
    bool spawned=false;
    for(const auto& u:game.units)if(u.ownerId==0 && u.position.x>-7 &&
                                   u.position.x<-5 && u.position.z>5)
        spawned=true;
    assert(spawned);
    game.update(1.0f/60.f);
    assert(game.playerMinerals==260);
    auto invalid=p;
    auto& a=invalid.extensions.at("triggers").get<picojson::array>();
    a.push_back(makeTrigger("invalid",{{"type",picojson::value("UnrestrictedCode")}},
        {picojson::value(picojson::object{{"type",picojson::value("GrantMinerals")}})}));
    bool rejected=false;
    try{TriggerEngine::validate(invalid);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    // Reject values that would be unsafe to cast or spawn outside the playable world.
    auto invalidSpawn=p;
    auto& first=invalidSpawn.extensions.at("triggers").get<picojson::array>();
    first.push_back(makeTrigger("outside",{{"type",picojson::value("After")},
                                           {"seconds",picojson::value(2.0)}},
        {picojson::value(picojson::object{{"type",picojson::value("SpawnUnit")},
            {"catalog",picojson::value("marine")},{"owner",picojson::value(0.0)},
            {"x",picojson::value(99999999.0)},{"z",picojson::value(0.0)}})}));
    rejected=false;
    try{TriggerEngine::validate(invalidSpawn);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::cout<<"triggers: start, delayed events, unit spawning and schema rejection passed\n";
}
