#pragma once
#include "third_party/picojson.h"
#include <string>
#include <unordered_set>
#include <vector>

class RTSProject;
class RTSSkirmish;

// A bounded, declarative trigger interpreter. Its event / condition / action
// graph is serializable JSON, intentionally not unrestricted native code.
class TriggerEngine {
public:
    struct Rule {
        std::string id;
        std::string event;
        std::string region;
        double interval=0;
        double threshold=0;
        double startTime=0;
        bool once=false;
        bool fired=false;
        std::vector<picojson::object> conditions;
        std::vector<picojson::object> actions;
        std::unordered_set<int> occupants;
    };
    void configure(const RTSProject& project);
    void start(RTSSkirmish& game);
    void tick(RTSSkirmish& game,double elapsed,const std::vector<int>& killed);
    const std::vector<Rule>& rules()const { return rules_; }
    static void validate(const RTSProject& project);
private:
    std::vector<Rule> rules_;
    std::vector<Rule> parse(const RTSProject& project)const;
    bool conditionsPass(const Rule& rule,const RTSSkirmish& game,int subjectId)const;
    void run(Rule& rule,RTSSkirmish& game,int subjectId);
};
