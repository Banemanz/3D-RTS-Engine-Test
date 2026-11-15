#pragma once
#include "NavigationGrid.hpp"
#include "RTSVision.hpp"
#include "UnitSpatialIndex.hpp"
#include "Unit.hpp"
#include "RTSProject.hpp"
#include "TriggerEngine.hpp"
#include "RTSCommandStream.hpp"
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
    struct CommandOutcome {
        std::uint64_t tick=0,sequence=0;
        RTSCommandKind kind=RTSCommandKind::Move;
        bool accepted=false;
        int source=0;
    };
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
    // Apply immutable gameplay orders at a fixed simulation tick. The UI
    // captures the selected entity IDs before the command is queued.
    bool queueCommand(RTSCommand command);
    bool queuePlayerCommand(RTSCommandKind kind,const Vec3& point=Vec3{},
                            int targetId=-1,bool append=false,
                            const std::string& catalog="");
    std::uint64_t simulationTick()const{return tick_;}
    std::string stateDigest()const;
    RTSReplayTape replayTape()const;
    bool replayRecordingValid()const{return replayValid_;}
    // Verification mode: v1 replays retain old direct orders; v2 uses only
    // their recorded AI inputs. Call only after loading a replay project.
    void setReplayAIMode(int formatVersion);
    // Small UI-observable result channel; never part of game-state hashing.
    std::vector<CommandOutcome> takeCommandOutcomes();
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
    std::size_t activePeriodicEffectCount()const{return periodicEffects_.size();}
    struct BehaviorWatch {
        int unitId=0;
        std::string behaviorId;
        int stacks=0;
        std::uint64_t remainingTicks=0;
    };
    std::size_t activeBehaviorCount()const{return behaviors_.size();}
    std::vector<BehaviorWatch> behaviorWatch(std::size_t limit=64)const;
    int unitBehaviorStacks(int unitId,const std::string& behaviorId)const;
    int unitBehaviorCount(int unitId)const;
    std::uint64_t unitBehaviorTicksRemaining(int unitId,
                                             const std::string& behaviorId)const;
    std::string selectedAbilityId(int slot) const;
    int abilityPage()const{return abilityPage_;}
    int abilityPageCount()const;
    bool nextAbilityPage();
    bool canPrepareAbility(int casterId,const std::string& abilityId) const;
    bool canCastAbility(int casterId,const std::string& abilityId,int targetId) const;
    bool canCastAbilityAt(int casterId,const std::string& abilityId,
                          const Vec3& ground) const;
    // Pure read-only render/monitor projection of pending simulation impacts.
    struct AreaImpactWatch {
        Vec3 center{};
        float radius=0.f;
        std::uint64_t remainingTicks=0;
        std::string abilityId;
        bool projectileVisible=false;
        Vec3 projectilePosition{};
        Vec3 projectileTrailStart{};
        Vec3 projectileTint{};
        float projectileScale=0.f;
    };
    struct ImpactFlashWatch {
        Vec3 center{};
        float radius=0.f;
        float remainingFraction=0.f;
        bool applied=false;
    };
    std::vector<ImpactFlashWatch> impactFlashWatch(std::size_t limit=32)const;
    std::vector<AreaImpactWatch> areaImpactWatch(std::size_t limit=32)const;
    std::size_t pendingAreaImpactCount()const{return pendingAreas_.size();}
    std::uint64_t abilityCooldownRemaining(int casterId,const std::string& abilityId) const;

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
    struct PeriodicEffect {
        int targetId=0;
        EffectOperationKind kind=EffectOperationKind::PeriodicDamage;
        float amount=0.f;
        std::uint32_t intervalTicks=0;
        std::uint64_t nextTick=0;
        std::uint64_t finalTick=0;
        std::string sourceEffect;
    };
    static constexpr std::size_t MaxPeriodicEffects=256;
    std::vector<PeriodicEffect> periodicEffects_;
    void updatePeriodicEffects();
    struct ActiveBehavior {
        int stacks=1;
        std::uint64_t expiresTick=0;
    };
    static constexpr std::size_t MaxActiveBehaviors=256;
    std::map<std::pair<int,std::string>,ActiveBehavior> behaviors_;
    void updateBehaviors();
    void recalculateBehaviorStats(int targetId);
    void applyBehavior(int targetId,const BehaviorDefinition& definition);
    void removeBehavior(int targetId,const std::string& id);
    std::map<std::pair<int,std::string>,std::uint64_t> abilityReadyAt_;
    struct PendingAreaImpact {
        Vec3 center{};
        float radius=0.f;
        int owner=0;
        AbilityAreaTeam team=AbilityAreaTeam::Enemy;
        std::string abilityId;
        std::string effectId;
        std::uint64_t impactTick=0;
        // Optional authored projectile delivery. Legacy timed areas keep
        // projectile=false and their exact prior impact scheduling.
        bool projectile=false;
        std::string projectileId;
        Vec3 launchPosition{};
        Vec3 destination{};
        std::uint64_t launchTick=0;
        std::uint64_t flightTicks=0;
        float arcHeight=0.f;
        float visualScale=0.f;
        Vec3 tint{};
        float collisionRadius=0.f;
        bool collideUnits=false;
    };
    struct ImpactFlash {
        Vec3 center{};
        float radius=0.f;
        std::uint64_t bornTick=0;
        bool applied=false;
    };
    std::deque<ImpactFlash> impactFlashes_;
    static constexpr std::uint64_t FlashTicks=24;
    static constexpr std::size_t MaxImpactFlashes=64;
    static Vec3 projectilePositionAt(const PendingAreaImpact& area,
                                     std::uint64_t atTick);
    bool detectProjectileCollision(PendingAreaImpact& area);
    static constexpr std::size_t MaxPendingAreaImpacts=128;
    std::vector<PendingAreaImpact> pendingAreas_;
    bool castAbility(int casterId,const std::string& abilityId,
                     int targetId,const Vec3& ground);
    bool applyAreaEffect(const PendingAreaImpact& area);
    void updateAreaImpacts();
    RTSCommandStream commandStream_;
    std::uint64_t tick_=0;
    bool replayValid_=true;
    bool applyingCommand_=false;
    enum class AIInputMode{Live,ReplayRecorded,LegacyDirect};
    AIInputMode aiInputMode_=AIInputMode::Live;
    std::deque<CommandOutcome> commandOutcomes_;
    std::vector<RTSCommand> recordedCommands_;
    std::vector<RTSReplayCheckpoint> checkpoints_;
    bool applyCommand(const RTSCommand& command);
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
    // UI-only command-card page. Not included in simulation digests.
    int abilityPage_ = 0;
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
    void planAIOrders();
    bool canQueueAt(int buildingId,const std::string& unitId) const;
    bool queueAt(int buildingId,const std::string& unitId);
    int countOwnerUnits(int owner) const;
    Unit& spawn(const UnitData& data, const Vec3& at, int owner, const std::string& name,
                bool structure = false, bool worker = false,
                const std::string& catalogId = "");
};
