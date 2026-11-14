#include "RTSSkirmish.hpp"
#include <cassert>
#include <iostream>

static picojson::value rule(const char* id,const char* event,double seconds,
                            double minerals,bool blocked=false) {
    picojson::object evt{{"type",picojson::value(event)}};
    if(std::string(event)=="Every" || std::string(event)=="After")
        evt["seconds"]=picojson::value(seconds);
    picojson::object obj{
        {"id",picojson::value(id)},
        {"event",picojson::value(evt)},
        {"once",picojson::value(std::string(event)!="Every")},
        {"actions",picojson::value(picojson::array{
            picojson::value(picojson::object{
                {"type",picojson::value("GrantMinerals")},
                {"amount",picojson::value(minerals)}
            })
        })}
    };
    if(blocked)obj["conditions"]=picojson::value(picojson::array{
        picojson::value(picojson::object{
            {"type",picojson::value("PlayerMineralsAtLeast")},
            {"amount",picojson::value(99999.0)}
        })
    });
    return picojson::value(obj);
}
int main(){
    RTSProject project=RTSProject::defaults();
    project.extensions["triggers"]=picojson::value(picojson::array{
        rule("on_start","OnMapStart",0.,5.),
        rule("blocked_start","OnMapStart",0.,999.,true),
        rule("each_tenth","Every",.1,1.)
    });
    RTSSkirmish traced;
    traced.setTriggerTracing(true);
    traced.loadProject(project);
    RTSSkirmish baseline;
    baseline.loadProject(project);
    const auto& engine=traced.triggerDiagnostics();
    assert(engine.trace().size()==2);
    assert(engine.trace()[0].ruleId=="on_start");
    assert(engine.trace()[0].status==TriggerEngine::TraceStatus::Executed);
    assert(engine.trace()[0].simulationSeconds==0.);
    assert(engine.trace()[0].detail=="GrantMinerals");
    assert(engine.trace()[1].ruleId=="blocked_start");
    assert(engine.trace()[1].status==TriggerEngine::TraceStatus::ConditionBlocked);
    assert(engine.trace()[1].detail.find("Condition 1")!=std::string::npos);
    assert(engine.rules()[0].attempts==1 && engine.rules()[0].executions==1);
    assert(engine.rules()[1].attempts==1 && engine.rules()[1].blocked==1);
    assert(traced.playerMinerals==baseline.playerMinerals);
    for(int i=0;i<120;++i){
        traced.update(1.f/60.f);
        baseline.update(1.f/60.f);
    }
    assert(traced.playerMinerals==baseline.playerMinerals);
    assert(traced.units.size()==baseline.units.size());
    assert(engine.rules()[2].executions>=16 && engine.rules()[2].executions<=20);
    assert(engine.trace().back().simulationSeconds>1.0);

    traced.triggerDiagnostics().clearTrace();
    assert(engine.trace().empty());
    assert(engine.rules()[2].executions>0);
    traced.setTriggerTracing(false);
    for(int i=0;i<15;++i)traced.update(1.f/60.f);
    assert(engine.trace().empty()); // disabling tracing does not disable scripts
    traced.setTriggerTracing(true);
    for(int i=0;i<15;++i)traced.update(1.f/60.f);
    assert(!engine.trace().empty());

    // A separate headless trigger interpreter verifies ring-buffer capacity
    // without requiring 300 real render frames or an OpenGL context.
    TriggerEngine probe;
    probe.enableTracing(true);
    probe.configure(project);
    probe.start(traced);
    for(int i=1;i<=320;++i)probe.tick(traced,100.+i*.2,{});
    assert(probe.trace().size()==TriggerEngine::TraceCapacity);
    assert(probe.trace().front().sequence>1);
    for(std::size_t i=1;i<probe.trace().size();++i)
        assert(probe.trace()[i-1].sequence<probe.trace()[i].sequence);
    assert(probe.trace().back().ruleId=="each_tenth");
    assert(probe.rules()[2].executions>=320);

    traced.reset();
    assert(engine.trace().size()==2); // reset preserves opt-in, drops prior history
    assert(engine.trace()[0].sequence==1);
    assert(engine.rules()[2].executions==0);
    assert(engine.rules()[1].blocked==1);
    std::cout<<"trigger tracing: opt-in, blocked rules, time, ring limit, reset, deterministic gameplay passed\n";
}
