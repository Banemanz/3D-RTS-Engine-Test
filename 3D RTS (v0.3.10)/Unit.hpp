#pragma once

#include "Math3D.hpp"
#include "Ray.hpp"
#include "NavigationGrid.hpp"
#include "UnitData.hpp"
#include <string>
#include <cstddef>
#include <vector>

class Unit;
class UnitSpatialIndex;
struct CombatHit {
    int attackerId=-1;
    int targetId=-1;
    float rawDamage=0.f;
    std::string attackerName;
};

class NavigationAgent3D {
public:
    Vec3 targetPosition{0.0f, 0.0f, 0.0f};
    bool hasDestination = false;

    float maxSpeed         = 9.5f;
    float stoppingDistance = 0.25f;
    float slowingRadius    = 2.2f;
    float separationRadius = 1.95f;
    float separationWeight = 11.5f;

    void setTargetPosition(const Vec3& target);
    void stop();
    bool isNavigationFinished(const Vec3& currentPos) const;
    Vec3 computeSteeringVelocity(const Unit& self, const std::vector<Unit>& allUnits,
                                  float deltaTime,const UnitSpatialIndex* index=nullptr);
};

enum class UnitState {
    IDLE,
    MOVING,
    CHASING,
    ATTACKING
};

inline std::string unitStateToString(UnitState s) {
    switch (s) {
        case UnitState::IDLE:      return "IDLE";
        case UnitState::MOVING:    return "MOVING";
        case UnitState::CHASING:   return "CHASING";
        case UnitState::ATTACKING: return "ATTACKING";
    }
    return "UNKNOWN";
}

class Unit {
public:
    int id = 0;
    // Control/ownership are separate from model faction/visuals.
    int ownerId = 0;
    bool isStructure = false;
    bool isWorker = false;
    std::string instanceName;
    std::string catalogId;
    UnitData data;

    Vec3 position{0.0f, 0.0f, 0.0f};
    float facingYawRad = 0.0f;
    float radius = 0.65f;
    float height = 1.6f;
    Vec3 modelScale{1,1,1};
    float actorOpacity = 1.0f;
    std::string actorMesh = "procedural.unit";

    float currentHp = 45.0f;
    bool isSelected = false;

    UnitState state = UnitState::IDLE;
    int targetUnitId = -1;
    float attackCooldownTimer = 0.0f;
    float attackFlashTimer = 0.0f;
    Vec3 lastAttackTargetPos{0.0f, 0.0f, 0.0f};
    bool attackMoveEnabled = false;
    bool holdPositionEnabled = false;

    NavigationAgent3D navAgent;
    // Borrowed from RTSSkirmish; do not delete or store across world destruction.
    const NavigationGrid* navGrid = nullptr;
    std::vector<Vec3> chaseWaypoints;
    std::size_t chaseWaypointIndex = 0;
    Vec3 chaseRouteGoal{0,0,0};
    float chaseRepathTimer = 0.0f;

    Unit(int id, const std::string& name, const UnitData& unitData, const Vec3& startPos);

    bool isAlive() const;
    Faction getFaction() const;
    void setSelected(bool selected);

    void moveTo(const Vec3& destination, bool isAttackMove = false);
    void attackTarget(int enemyId);
    void holdPosition();
    void takeDamage(float rawDamage, const std::string& attackerName);

    void update(float deltaTime, std::vector<Unit>& allUnits, float mapBoundary,
                const UnitSpatialIndex* spatial=nullptr,
                std::vector<CombatHit>* pendingHits=nullptr);
    bool intersectRay(const Ray& ray, float& outDistance) const;

private:
    Vec3 currentVelocity{0.0f, 0.0f, 0.0f};

    void smoothRotateToward(float targetYaw, float deltaTime);
    static Unit* findUnitById(std::vector<Unit>& allUnits, int searchId,
                              const UnitSpatialIndex* spatial);
    int findNearestEnemyId(const std::vector<Unit>& allUnits,float searchRadius,
                           const UnitSpatialIndex* spatial) const;
    void updateCombatStateMachine(float deltaTime,std::vector<Unit>& allUnits,
                                  const UnitSpatialIndex* spatial,
                                  std::vector<CombatHit>* pendingHits);
};