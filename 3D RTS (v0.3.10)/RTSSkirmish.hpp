#pragma once
#include "NavigationGrid.hpp"
#include "Unit.hpp"
#include <cstddef>
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
    void reset();
    void update(float fixedDt);

    std::vector<Unit> units;
    std::vector<MineralNode> minerals;
    NavigationGrid navigation;
    int playerMinerals = 0;
    int supplyUsed = 0;
    int supplyCap = 0;
    MatchResult result = MatchResult::Playing;

    bool orderMove(const Vec3& destination, bool attackMove);
    bool orderGather(const Vec3& nearNode);
    bool orderAttack(int targetId);
    void orderStop();
    bool buildBarracks(const Vec3& at);
    bool queueMarine();
    bool queueWorker();
    std::string statusLine() const;
    int activeWorkerCount() const;
    int completedBarracksCount() const;

private:
    struct Route {
        std::vector<Vec3> points;
        size_t next = 0;
        bool attackMove = false;
    };
    enum class WorkerPhase { ToMineral, Harvesting, ToBase };
    struct WorkerJob {
        size_t mineralIndex = 0;
        WorkerPhase phase = WorkerPhase::ToMineral;
        float timer = 0.0f;
        int cargo = 0;
    };
    struct Production {
        int buildingId = -1;
        bool worker = false;
        float remaining = 0.0f;
        int reservedSupply = 1;
        int paidMinerals = 0;
    };
    int nextId_ = 1;
    float enemySpawnTimer_ = 12.0f;
    std::unordered_set<int> releasedSupply_;
    std::unordered_map<int, Route> routes_;
    std::unordered_map<int, WorkerJob> jobs_;
    std::unordered_map<int, float> construction_;
    std::vector<Production> production_;

    Unit* find(int id);
    const Unit* find(int id) const;
    int friendlyBaseId_ = -1;
    int enemyBaseId_ = -1;
    bool setRoute(Unit& u, const Vec3& to, bool attackMove);
    void advanceRoutes();
    void updateGathering(float dt);
    void updateProduction(float dt);
    void updateEnemy(float dt);
    bool queueAt(int buildingId, bool worker, int cost, float seconds, int supply);
    int countOwnerUnits(int owner) const;
    Unit& spawn(const UnitData& data, const Vec3& at, int owner, const std::string& name,
                bool structure = false, bool worker = false);
};
