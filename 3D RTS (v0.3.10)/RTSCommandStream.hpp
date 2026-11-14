#pragma once
#include "RTSProject.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Simulated gameplay commands, not graphical UI events. A command carries
// explicit entity IDs so later selection changes cannot affect its meaning.
enum class RTSCommandKind {
    Move, AttackMove, Patrol, Gather, Attack, Stop, Hold,
    Build, Rally, TrainCatalog, TrainRanger, TrainWorker,
    CancelTraining, Context
};
struct RTSCommand {
    std::uint64_t tick=0;
    std::uint64_t sequence=0;
    int source=0; // currently local human 0; retained for future command routing
    RTSCommandKind kind=RTSCommandKind::Move;
    std::vector<int> units;
    Vec3 point{};
    int targetId=-1;
    bool append=false;
    std::string catalog;
};
struct RTSReplayCheckpoint {
    std::uint64_t tick=0;
    std::string digest; // canonical lowercase 16-digit hex
};

class RTSCommandStream {
public:
    static constexpr std::uint64_t MaxCommandTick=9007199254740991ULL;
    static constexpr std::size_t MaxPending=512;
    static constexpr std::size_t MaxUnitIds=128;
    static constexpr std::uint64_t MaxAhead=1200;
    bool enqueue(RTSCommand command,std::uint64_t currentTick);
    std::vector<RTSCommand> take(std::uint64_t tick);
    void clear();
    std::size_t pending()const{return pending_;}
private:
    std::map<std::uint64_t,std::vector<RTSCommand>> scheduled_;
    std::uint64_t nextSequence_=1;
    std::size_t pending_=0;
};
const char* rtsCommandName(RTSCommandKind kind);
void validateRTSCommand(const RTSCommand& command);
picojson::value commandToJson(const RTSCommand& command);
RTSCommand commandFromJson(const picojson::value& root);

struct RTSReplayTape {
    static constexpr std::uint64_t MaxTicks=5184000; // 24 hours at 60Hz
    RTSProject project;
    std::uint64_t totalTicks=0;
    std::vector<RTSCommand> commands;
    std::vector<RTSReplayCheckpoint> checkpoints;
    picojson::value toJson()const;
    static RTSReplayTape fromJson(const picojson::value& value);
    static RTSReplayTape loadFile(const std::string& path);
    void saveFile(const std::string& path)const;
};

// Reconstruct the saved map, reissue commands at recorded ticks and verify
// every checkpoint. Never require a window, GPU, OS clock or network.
bool verifyRTSReplay(const RTSReplayTape& tape,std::string* diagnostic=nullptr);
