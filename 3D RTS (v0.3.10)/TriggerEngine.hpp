#pragma once
#include "third_party/picojson.h"
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

class RTSProject;
class RTSSkirmish;

// A bounded, declarative trigger interpreter. Its event / condition / action
// graph is serializable JSON, intentionally not unrestricted native code.
class TriggerEngine {
public:
    enum class TraceStatus { Executed, ConditionBlocked, ActionError };
    struct Trace {
        std::uint64_t sequence=0;
        std::string ruleId, event, detail;
        double simulationSeconds=0.0;
        int subjectId=-1;
        TraceStatus status=TraceStatus::Executed;
    };
    static constexpr std::size_t TraceCapacity=192;
    struct Rule {
        std::string id;
        std::string event;
        std::string region;
        double interval=0;
        double threshold=0;
        double startTime=0;
        bool once=false;
        bool fired=false;
        // Run-only telemetry; not serialized into project assets.
        std::uint64_t attempts=0;
        std::uint64_t executions=0;
        std::uint64_t blocked=0;
        std::vector<picojson::object> conditions;
        std::vector<picojson::object> actions;
        std::unordered_set<int> occupants;
    };
    void configure(const RTSProject& project);
    void start(RTSSkirmish& game);
    void tick(RTSSkirmish& game,double elapsed,const std::vector<int>& killed);
    const std::vector<Rule>& rules()const { return rules_; }
    void enableTracing(bool value){tracingEnabled_=value;}
    bool tracingEnabled()const{return tracingEnabled_;}
    const std::deque<Trace>& trace()const{return trace_;}
    void clearTrace(){trace_.clear();}
    double lastSimulationSeconds()const{return lastSimulationSeconds_;}
    static void validate(const RTSProject& project);
private:
    std::vector<Rule> rules_;
    bool tracingEnabled_=false;
    std::deque<Trace> trace_;
    std::uint64_t nextSequence_=1;
    double lastSimulationSeconds_=0.0;
    std::vector<Rule> parse(const RTSProject& project)const;
    bool conditionsPass(const Rule& rule,const RTSSkirmish& game,int subjectId,
                        std::size_t* failedCondition)const;
    void record(const Rule& rule,double elapsed,int subjectId,
                TraceStatus status,const std::string& detail);
    void run(Rule& rule,RTSSkirmish& game,int subjectId,double elapsed);
};
