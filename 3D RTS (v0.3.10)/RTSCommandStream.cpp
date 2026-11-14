#include "RTSCommandStream.hpp"
#include "RTSSkirmish.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
constexpr std::array<const char*,15> names{{
    "Move","AttackMove","Patrol","Gather","Attack","Stop","Hold",
    "Build","Rally","TrainCatalog","TrainRanger","TrainWorker",
    "CancelTraining","Context","CastAbility"
}};
std::uint64_t integer(const picojson::object& object,const char* key,
                      std::uint64_t minimum,std::uint64_t maximum) {
    const auto it=object.find(key);
    if(it==object.end() || !it->second.is<double>())
        throw std::invalid_argument(std::string("Missing integer: ")+key);
    const double n=it->second.get<double>();
    if(!std::isfinite(n) || std::floor(n)!=n ||
       n<double(minimum) || n>double(maximum))
        throw std::invalid_argument(std::string("Invalid integer: ")+key);
    return static_cast<std::uint64_t>(n);
}
const picojson::value& required(const picojson::object& object,const char* key) {
    auto it=object.find(key);
    if(it==object.end())throw std::invalid_argument(std::string("Missing: ")+key);
    return it->second;
}
std::string string(const picojson::object& object,const char* key){
    const auto& value=required(object,key);
    if(!value.is<std::string>())throw std::invalid_argument(std::string("Invalid string: ")+key);
    return value.get<std::string>();
}
picojson::array pointToJson(const Vec3& point){
    return {picojson::value(double(point.x)),picojson::value(double(point.z))};
}
Vec3 pointFromJson(const picojson::value& input){
    if(!input.is<picojson::array>() || input.get<picojson::array>().size()!=2)
        throw std::invalid_argument("Command point must be an [x,z] pair");
    const auto& p=input.get<picojson::array>();
    if(!p[0].is<double>() || !p[1].is<double>())
        throw std::invalid_argument("Command coordinates must be numbers");
    return {float(p[0].get<double>()),0.f,float(p[1].get<double>())};
}
bool commandLess(const RTSCommand& a,const RTSCommand& b){
    if(a.tick!=b.tick)return a.tick<b.tick;
    if(a.source!=b.source)return a.source<b.source;
    return a.sequence<b.sequence;
}
}
const char* rtsCommandName(RTSCommandKind kind){
    const auto index=static_cast<std::size_t>(kind);
    return index<names.size()?names[index]:"Unknown";
}
void validateRTSCommand(const RTSCommand& c){
    if(c.tick==0 || c.tick>RTSCommandStream::MaxCommandTick)
        throw std::invalid_argument("Command tick outside supported replay duration");
    if(c.sequence>1000000000ULL || (c.source!=0 && c.source!=1) ||
       static_cast<unsigned>(c.kind)>=names.size() ||
       c.units.size()>RTSCommandStream::MaxUnitIds ||
       c.catalog.size()>128 || c.targetId < -1 || c.targetId>10000000 ||
       !std::isfinite(c.point.x) || !std::isfinite(c.point.y) ||
       !std::isfinite(c.point.z) || std::fabs(c.point.x)>100000.f ||
       std::fabs(c.point.z)>100000.f)
        throw std::invalid_argument("Invalid command parameters");
    if(c.kind==RTSCommandKind::CastAbility &&
       (c.catalog.empty() || c.units.empty() || c.append))
        throw std::invalid_argument("Ability command requires a catalog ID, caster, and no queue append");
    // Internal AI has one explicitly authorized action. This prevents
    // game-owned input from issuing human-only economy/production orders.
    if(c.source==1 && (c.kind!=RTSCommandKind::Attack ||
       c.targetId<1 || c.units.empty()))
        throw std::invalid_argument("AI commands require an attack target and unit IDs");
    for(std::size_t i=0;i<c.units.size();++i)
        if(c.units[i]<1 || c.units[i]>10000000 ||
           (i>0 && c.units[i-1]>=c.units[i]))
            throw std::invalid_argument("Command unit IDs must be unique and ascending");
}
bool RTSCommandStream::enqueue(RTSCommand command,std::uint64_t currentTick){
    validateRTSCommand(command);
    if(command.tick<=currentTick || command.tick-currentTick>MaxAhead ||
       pending_>=MaxPending)return false;
    auto& batch=scheduled_[command.tick];
    if(batch.size()>=64)return false;
    if(command.sequence==0){
        if(nextSequence_>1000000000ULL)return false;
        command.sequence=nextSequence_++;
    }else nextSequence_=std::max(nextSequence_,command.sequence+1);
    const auto same=[&](const RTSCommand& e){
        return e.source==command.source && e.sequence==command.sequence;
    };
    if(std::any_of(batch.begin(),batch.end(),same))return false;
    batch.push_back(std::move(command));
    ++pending_;
    return true;
}
std::vector<RTSCommand> RTSCommandStream::take(std::uint64_t tick){
    auto it=scheduled_.find(tick);
    if(it==scheduled_.end())return {};
    auto result=std::move(it->second);
    pending_-=result.size();
    scheduled_.erase(it);
    std::sort(result.begin(),result.end(),commandLess);
    return result;
}
void RTSCommandStream::clear(){
    scheduled_.clear();pending_=0;nextSequence_=1;
}
picojson::value commandToJson(const RTSCommand& c){
    validateRTSCommand(c);
    picojson::array unitIds;
    for(int id:c.units)unitIds.emplace_back(double(id));
    return picojson::value(picojson::object{
        {"tick",picojson::value(double(c.tick))},
        {"sequence",picojson::value(double(c.sequence))},
        {"source",picojson::value(double(c.source))},
        {"kind",picojson::value(names[static_cast<std::size_t>(c.kind)])},
        {"units",picojson::value(unitIds)},
        {"point",picojson::value(pointToJson(c.point))},
        {"target",picojson::value(double(c.targetId))},
        {"append",picojson::value(c.append)},
        {"catalog",picojson::value(c.catalog)}
    });
}
RTSCommand commandFromJson(const picojson::value& value){
    if(!value.is<picojson::object>())throw std::invalid_argument("Command must be object");
    const auto& o=value.get<picojson::object>();
    RTSCommand c;
    c.tick=integer(o,"tick",1,RTSCommandStream::MaxCommandTick);
    c.sequence=integer(o,"sequence",1,1000000000ULL);
    c.source=int(integer(o,"source",0,1));
    const auto kind=string(o,"kind");
    auto it=std::find_if(names.begin(),names.end(),[&](const char* name){return kind==name;});
    if(it==names.end())throw std::invalid_argument("Unknown command kind");
    c.kind=static_cast<RTSCommandKind>(std::distance(names.begin(),it));
    const auto& ids=required(o,"units");
    if(!ids.is<picojson::array>() || ids.get<picojson::array>().size()>RTSCommandStream::MaxUnitIds)
        throw std::invalid_argument("Invalid command unit ID list");
    for(const auto& id:ids.get<picojson::array>()){
        if(!id.is<double>() || !std::isfinite(id.get<double>()) ||
           std::floor(id.get<double>())!=id.get<double>() ||
           id.get<double>()<1 || id.get<double>()>10000000)
            throw std::invalid_argument("Invalid command unit ID");
        c.units.push_back(int(id.get<double>()));
    }
    c.point=pointFromJson(required(o,"point"));
    const auto& target=required(o,"target");
    if(!target.is<double>() || !std::isfinite(target.get<double>()) ||
       std::floor(target.get<double>())!=target.get<double>() ||
       target.get<double>()< -1 || target.get<double>()>10000000)
        throw std::invalid_argument("Invalid target");
    c.targetId=int(target.get<double>());
    const auto& append=required(o,"append");
    if(!append.is<bool>())throw std::invalid_argument("Invalid append flag");
    c.append=append.get<bool>();
    c.catalog=string(o,"catalog");
    validateRTSCommand(c);
    return c;
}
picojson::value RTSReplayTape::toJson()const{
    if((version!=1 && version!=2) || totalTicks>MaxTicks || commands.size()>MaxTicks*2 ||
       checkpoints.size()>MaxTicks/60+2)
        throw std::invalid_argument("Replay exceeds bounded limits");
    project.validate();
    picojson::array list;
    RTSCommand previous{};
    for(const auto& command:commands){
        if(command.tick>totalTicks || (!list.empty() &&
            (!commandLess(previous,command) ||
             (previous.tick==command.tick && previous.source==command.source &&
              previous.sequence==command.sequence))))
            throw std::invalid_argument("Replay commands are not strictly ordered");
        if(version==1 && command.source!=0)
            throw std::invalid_argument("Version 1 replays cannot contain AI commands");
        list.push_back(commandToJson(command));
        previous=command;
    }
    picojson::array samples;
    std::uint64_t previousTick=0;
    for(const auto& check:checkpoints){
        if(check.tick>totalTicks || (!samples.empty() && check.tick<=previousTick) ||
           check.digest.size()!=16 || !std::all_of(check.digest.begin(),check.digest.end(),
               [](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
            throw std::invalid_argument("Invalid replay checkpoint");
        samples.emplace_back(picojson::object{
            {"tick",picojson::value(double(check.tick))},
            {"digest",picojson::value(check.digest)}
        });
        previousTick=check.tick;
    }
    return picojson::value(picojson::object{
        {"version",picojson::value(double(version))},
        {"tick_hz",picojson::value(60.0)},
        {"ticks",picojson::value(double(totalTicks))},
        {"project",project.toJson()},
        {"commands",picojson::value(list)},
        {"checkpoints",picojson::value(samples)}
    });
}
RTSReplayTape RTSReplayTape::fromJson(const picojson::value& input){
    if(!input.is<picojson::object>())throw std::invalid_argument("Replay must be JSON object");
    const auto& o=input.get<picojson::object>();
    const int format=int(integer(o,"version",1,2));
    if(integer(o,"tick_hz",60,60)!=60)
        throw std::invalid_argument("Unsupported replay version or tick rate");
    RTSReplayTape tape;
    tape.version=format;
    tape.totalTicks=integer(o,"ticks",0,MaxTicks);
    tape.project=RTSProject::fromJson(required(o,"project"));
    tape.project.validate();
    const auto& commands=required(o,"commands");
    const auto& checkpoints=required(o,"checkpoints");
    if(!commands.is<picojson::array>() || !checkpoints.is<picojson::array>() ||
       commands.get<picojson::array>().size()>MaxTicks*2 ||
       checkpoints.get<picojson::array>().size()>MaxTicks/60+2)
        throw std::invalid_argument("Invalid replay collection sizes");
    for(const auto& entry:commands.get<picojson::array>())
        tape.commands.push_back(commandFromJson(entry));
    for(const auto& entry:checkpoints.get<picojson::array>()){
        if(!entry.is<picojson::object>())
            throw std::invalid_argument("Invalid checkpoint");
        const auto& item=entry.get<picojson::object>();
        tape.checkpoints.push_back({integer(item,"tick",0,MaxTicks),
                                    string(item,"digest")});
    }
    (void)tape.toJson(); // replay format, order, and checksum validation
    return tape;
}
RTSReplayTape RTSReplayTape::loadFile(const std::string& path){
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in)throw std::runtime_error("Cannot open replay file: "+path);
    const auto size=in.tellg();
    if(size<0 || size>32*1024*1024)
        throw std::invalid_argument("Replay file exceeds 32 MiB");
    in.seekg(0);
    const std::string data(std::istreambuf_iterator<char>{in},
                           std::istreambuf_iterator<char>{});
    picojson::value value;
    const std::string error=picojson::parse(value,data);
    if(!error.empty())throw std::invalid_argument("Invalid replay JSON: "+error);
    return fromJson(value);
}
void RTSReplayTape::saveFile(const std::string& path)const{
    const std::string data=toJson().serialize();
    if(data.size()>32*1024*1024)
        throw std::invalid_argument("Replay exceeds 32 MiB");
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("Cannot write replay file: "+path);
    out<<data<<"\n";
    if(!out)throw std::runtime_error("Replay file write failed");
}

bool verifyRTSReplay(const RTSReplayTape& tape,std::string* diagnostic){
    const auto fail=[&](const std::string& reason){
        if(diagnostic)*diagnostic=reason;
        return false;
    };
    try {
        (void)tape.toJson(); // strict schema, command ordering and bounds
        RTSSkirmish game;
        game.loadProject(tape.project);
        // Version 1 depended on the old direct-AI behaviour. Version 2
        // supplies recorded AI decisions and must not plan duplicates.
        game.setReplayAIMode(tape.version);
        std::size_t commandIndex=0,checkpointIndex=0;
        const auto verifyTick=[&](std::uint64_t tick)->bool{
            if(checkpointIndex<tape.checkpoints.size() &&
               tape.checkpoints[checkpointIndex].tick==tick){
                const std::string actual=game.stateDigest();
                const auto& check=tape.checkpoints[checkpointIndex++];
                if(check.digest!=actual)
                    return fail("Replay diverged at tick "+std::to_string(tick)+
                                ": expected "+check.digest+" got "+actual);
            }
            return true;
        };
        if(!verifyTick(0))return false;
        for(std::uint64_t tick=1;tick<=tape.totalTicks;++tick){
            while(commandIndex<tape.commands.size() &&
                  tape.commands[commandIndex].tick==tick){
                if(!game.queueCommand(tape.commands[commandIndex++]))
                    return fail("Rejected command at replay tick "+std::to_string(tick));
            }
            game.update(1.f/60.f);
            if(game.simulationTick()!=tick)
                return fail("Replay simulation ended early at tick "+std::to_string(tick));
            if(!verifyTick(tick))return false;
        }
        if(commandIndex!=tape.commands.size() ||
           checkpointIndex!=tape.checkpoints.size())
            return fail("Replay has commands or checkpoints outside executed timeline");
        if(diagnostic)*diagnostic="Replay verified for "+
                                  std::to_string(tape.totalTicks)+" simulation ticks";
        return true;
    }catch(const std::exception& e){return fail(e.what());}
}
