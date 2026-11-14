#pragma once
#include "third_party/picojson.h"
#include <cstdint>
#include <deque>
#include <map>
#include <utility>
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
        std::string variable; // OnVariableChanged event subscription
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
    const std::map<std::string,int>& variables()const{return variables_;}
    // First and last value within one tick; a variable fires at most once on
    // the next tick. Included in replay digests because it affects future work.
    const std::map<std::string,std::pair<int,int>>& pendingVariableChanges()const{
        return pendingVariableChanges_;
    }
    void enableTracing(bool value){tracingEnabled_=value;}
    bool tracingEnabled()const{return tracingEnabled_;}
    const std::deque<Trace>& trace()const{return trace_;}
    void clearTrace(){trace_.clear();}
    double lastSimulationSeconds()const{return lastSimulationSeconds_;}
    static void validate(const RTSProject& project);
private:
    std::vector<Rule> rules_;
    std::map<std::string,int> variables_; // sorted for stable replay/inspectors
    std::map<std::string,std::pair<int,int>> pendingVariableChanges_;
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
    void executeActions(const std::vector<picojson::object>& actions,
                        RTSSkirmish& game,int subjectId,
                        std::string& summary,std::size_t& steps,int depth);
};
