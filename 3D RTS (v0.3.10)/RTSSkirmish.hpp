#pragma once
#include "NavigationGrid.hpp"
#include "RTSVision.hpp"
#include "UnitSpatialIndex.hpp"
#include "Unit.hpp"
#include "RTSProject.hpp"
#include "TriggerEngine.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct MineralNode {
    Vec3 position;
    int remaining = 0;
};

enum class MatchResult { Playing, Victory, Defeat };

// Owns the playable demo's simulation and the integer-ID references that survive
// vector reallocation. GPU objects and mouse input deliberately live elsewhere.
class RTSSkirmish {
public:
    RTSSkirmish();
    RTSSkirmish(const RTSSkirmish&) = delete;
    RTSSkirmish& operator=(const RTSSkirmish&) = delete;
    RTSSkirmish(RTSSkirmish&&) = delete;
    RTSSkirmish& operator=(RTSSkirmish&&) = delete;
    void reset();
    void loadProject(const RTSProject& project);
    void syncTerrain(const TerrainMap& terrain);
    const RTSProject& project() const { return project_; }
    void update(float fixedDt);
    // Playtest diagnostics are ephemeral, never saved to authored map data.
    void setTriggerTracing(bool enabled){triggers_.enableTracing(enabled);}
    const TriggerEngine& triggerDiagnostics()const{return triggers_;}
    TriggerEngine& triggerDiagnostics(){return triggers_;}

    std::vector<Unit> units;
    std::vector<MineralNode> minerals;
    NavigationGrid navigation;
    RTSVision vision; // player team's explored/visible terrain, independent of OpenGL
    int playerMinerals = 0;
    int supplyUsed = 0;
    int supplyCap = 0;
    MatchResult result = MatchResult::Playing;
    std::string triggerMessage;

    bool orderMove(const Vec3& destination, bool attackMove, bool append = false);
    bool orderPatrol(const Vec3& destination);
    bool setRallyPoint(const Vec3& destination);
    const std::unordered_map<int,Vec3>& rallyPoints() const { return rallyPoints_; }
    int pendingProduction(int buildingId) const;
    float productionProgress(int buildingId) const;
    bool cancelSelectedProduction();
    bool orderGather(const Vec3& nearNode);
    bool orderAttack(int targetId);
    void orderStop();
    void orderHold();
    bool buildBarracks(const Vec3& at);
    bool queueRanger();
    bool queueWorker();
    // Per-building, inherited Unit catalog trainable_units lists, 0..12 entries.
    std::vector<std::string> trainableUnits(int buildingId) const;
    int selectedProducerId() const;
    int trainingPage() const;
    int trainingPageCount() const;
    bool nextTrainingPage();
    std::string selectedTrainableId(int slot) const;
    bool canQueueSelectedTrainingSlot(int slot) const;
    bool queueSelectedTrainingSlot(int slot);
    std::string statusLine() const;
    int activeWorkerCount() const;
    int completedBarracksCount() const;
    int playerBaseId() const { return friendlyBaseId_; }

    // Script actions expose only explicit, bounded game-state mutations.
    void addMineralsForTrigger(int amount);
    // The trigger supplies a subject from an OnRegionEnter event. The
    // authored Effect catalog graph remains bounded/validated headlessly.
    bool applyEffectForTrigger(const std::string& effectId,int subjectId);
    bool spawnForTrigger(const std::string& catalogId,int owner,const Vec3& at);
    void endMatchForTrigger(bool victory);
    void showMessageForTrigger(const std::string& message);
    int countUnitsForTrigger(int owner,const std::string& catalogId) const;
    int ownerForTrigger(int unitId) const;
    const ProjectPlacement* regionForTrigger(const std::string& id) const;

private:
    RTSProject project_;
    UnitSpatialIndex spatial_; // incrementally updated every fixed simulation tick
    std::string rangerId_="ranger",workerId_="worker",enemyId_="rift_raider",barracksId_="barracks";
    std::string playerBaseCatalog_="command_center",enemyBaseCatalog_="rift_bastion";
    float enemyInterval_=16.0f;
    double elapsedSeconds_=0.0;
    std::unordered_set<int> observedDead_;
    TriggerEngine triggers_;
    void rebuildNavigation();
    std::uint64_t navigationGeneration_=0;
    struct Route {
        std::vector<Vec3> points;
        Vec3 requestedGoal{};
        std::uint64_t navigationGeneration=0;
        std::uint64_t gridRevision=0;
        size_t next = 0;
        bool attackMove = false;
        bool engagementInterrupted = false; // resume destination after combat
        bool patrol = false;
        bool returning = false;
        Vec3 patrolStart{};
        Vec3 patrolGoal{};
    };
    enum class WorkerPhase { ToMineral, Harvesting, ToBase };
    struct WorkerJob {
        size_t mineralIndex = 0;
        WorkerPhase phase = WorkerPhase::ToMineral;
        float timer = 0.0f;
        int cargo = 0;
    };
    struct PendingMove { Vec3 target; bool attackMove = false; };
    struct Production {
        int buildingId = -1;
        std::string catalogId;
        float remaining = 0.0f;
        int reservedSupply = 1;
        int paidMinerals = 0;
        float totalTime = 0.f;
    };
    int nextId_ = 1;
    bool occupancyDirty_ = false;
    int trainingPage_ = 0;
    float enemySpawnTimer_ = 12.0f;
    float visionIntervalTimer_=0.0f;
    std::unordered_set<int> releasedSupply_;
    std::map<int, Route> routes_; // stable unit-ID processing order
    std::unordered_map<int, std::deque<PendingMove>> queued_;
    std::map<int, WorkerJob> jobs_; // deterministic shared mineral contention
    std::unordered_map<int, float> construction_;
    std::vector<Production> production_;
    std::unordered_map<int,Vec3> rallyPoints_;

    Unit* find(int id);
    const Unit* find(int id) const;
    int friendlyBaseId_ = -1;
    int enemyBaseId_ = -1;
    bool setRoute(Unit& u, const Vec3& to, bool attackMove);
    void advanceRoutes();
    void updateGathering(float dt);
    void updateProduction(float dt);
    bool findProductionSpawn(const Unit& producer,const UnitData& type,Vec3& out) const;
    void updateEnemy(float dt);
    bool canQueueAt(int buildingId,const std::string& unitId) const;
    bool queueAt(int buildingId,const std::string& unitId);
    int countOwnerUnits(int owner) const;
    Unit& spawn(const UnitData& data, const Vec3& at, int owner, const std::string& name,
                bool structure = false, bool worker = false,
                const std::string& catalogId = "");
};
