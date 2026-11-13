#include "Unit.hpp"
#include "UnitSpatialIndex.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

void NavigationAgent3D::setTargetPosition(const Vec3& target) {
    targetPosition = Vec3(target.x, 0.0f, target.z);
    hasDestination = true;
}

void NavigationAgent3D::stop() {
    hasDestination = false;
}

bool NavigationAgent3D::isNavigationFinished(const Vec3& currentPos) const {
    if (!hasDestination) return true;
    Vec3 toTarget = targetPosition - currentPos;
    toTarget.y = 0.0f;
    return toTarget.length() <= stoppingDistance;
}

Vec3 NavigationAgent3D::computeSteeringVelocity(const Unit& self,
      const std::vector<Unit>& allUnits,float /*deltaTime*/,
      const UnitSpatialIndex* spatial) {
    Vec3 seekForce(0.0f, 0.0f, 0.0f);

    if (hasDestination) {
        Vec3 toTarget = targetPosition - self.position;
        toTarget.y = 0.0f;
        float dist = toTarget.length();

        if (dist <= stoppingDistance) {
            hasDestination = false;
        } else {
            float speed = maxSpeed;
            if (dist < slowingRadius) {
                speed = maxSpeed * (dist / slowingRadius);
            }
            seekForce = toTarget.normalized() * speed;
        }
    }

    Vec3 separationForce(0.0f, 0.0f, 0.0f);
    int neighborCount = 0;

    auto consider = [&](const Unit& other) {
        if (other.id == self.id || !other.isAlive()) return;
        float reach = separationRadius + (other.isStructure ? other.radius : 0.0f);

        Vec3 away = self.position - other.position;
        away.y = 0.0f;
        float dist = away.length();

        if (dist < reach) {
            if (dist < 1e-4f) {
                float angle = static_cast<float>(self.id * 137.5f) * (PI / 180.0f);
                away = Vec3(std::cos(angle), 0.0f, std::sin(angle));
                dist = 0.01f;
            }

            float strength = (reach - dist) / reach;
            separationForce += away.normalized() * (strength * separationWeight);
            neighborCount++;
        }
    };
    if(spatial) {
        for(auto idx:spatial->nearby(self.position,
             separationRadius+spatial->maxStructureRadius()))
            consider(allUnits[idx]);
    }else{
        for(const auto& other:allUnits)consider(other);
    }

    Vec3 combined = seekForce + separationForce;
    if (combined.lengthXZ() > maxSpeed) {
        combined = combined.normalized() * maxSpeed;
    }

    if (!hasDestination && neighborCount == 0) {
        return Vec3(0.0f, 0.0f, 0.0f);
    }

    return combined;
}

Unit::Unit(int id, const std::string& name, const UnitData& unitData, const Vec3& startPos)
    : id(id), instanceName(name), data(unitData), position(startPos) {
    currentHp = data.max_hp;
    radius = 0.62f * data.visual_scale;
    height = 1.55f * data.visual_scale;
    navAgent.maxSpeed = data.move_speed;
    navAgent.separationRadius = 1.85f * data.visual_scale;
}

bool Unit::isAlive() const {
    return currentHp > 0.0f;
}

Faction Unit::getFaction() const {
    return data.faction;
}

void Unit::setSelected(bool selected) {
    isSelected = selected;
}

void Unit::moveTo(const Vec3& destination, bool isAttackMove) {
    targetUnitId = -1;
    chaseWaypoints.clear();
    chaseWaypointIndex = 0;
    attackMoveEnabled = isAttackMove;
    holdPositionEnabled = false;
    state = UnitState::MOVING;
    navAgent.setTargetPosition(destination);
}

void Unit::attackTarget(int enemyId) {
    targetUnitId = enemyId;
    chaseWaypoints.clear();
    chaseWaypointIndex = 0;
    attackMoveEnabled = false;
    holdPositionEnabled = false;
    state = UnitState::CHASING;
}

void Unit::holdPosition() {
    targetUnitId = -1;
    chaseWaypoints.clear();
    chaseWaypointIndex = 0;
    chaseRepathTimer = 0.0f;
    attackMoveEnabled = false;
    holdPositionEnabled = true;
    state = UnitState::IDLE;
    navAgent.stop();
    currentVelocity = Vec3(0.0f, 0.0f, 0.0f);
}

void Unit::takeDamage(float rawDamage, const std::string& attackerName) {
    if (!isAlive()) return;
    float mitigated = std::max(1.0f, rawDamage - data.armor);
    currentHp = std::max(0.0f, currentHp - mitigated);

    if (currentHp <= 0.0f) {
        isSelected = false;
        navAgent.stop();
        std::cout << "[Combat] " << instanceName << " (" << data.resourceName
                  << ") was eliminated by " << attackerName << "!" << std::endl;
    }
}

void Unit::update(float deltaTime,std::vector<Unit>& allUnits,float mapBoundary,
                  const UnitSpatialIndex* spatial,std::vector<CombatHit>* pendingHits) {
    if (!isAlive() || isStructure) return;

    if (attackCooldownTimer > 0.0f) {
        attackCooldownTimer = std::max(0.0f, attackCooldownTimer - deltaTime);
    }
    if (attackFlashTimer > 0.0f) {
        attackFlashTimer = std::max(0.0f, attackFlashTimer - deltaTime);
    }

    updateCombatStateMachine(deltaTime,allUnits,spatial,pendingHits);

    // Hold Position is a hard movement constraint, not a slow/stop order.
    // The unit can rotate and fire, but neither inertia nor separation drifts it.
    if(holdPositionEnabled) {
        currentVelocity = Vec3(0.0f,0.0f,0.0f);
        navAgent.stop();
        if(state==UnitState::ATTACKING && targetUnitId!=-1) {
            Unit* target=findUnitById(allUnits,targetUnitId,spatial);
            if(target && target->isAlive()) {
                const Vec3 toward=target->position-position;
                if(toward.lengthXZ()>.01f)
                    smoothRotateToward(std::atan2(toward.x,toward.z),deltaTime);
            }
        }
        return;
    }

    Vec3 desiredVel = navAgent.computeSteeringVelocity(*this,allUnits,deltaTime,spatial);

    float blend = 1.0f - std::exp(-14.0f * deltaTime);
    currentVelocity = currentVelocity + (desiredVel - currentVelocity) * blend;

    position += currentVelocity * deltaTime;
    position.y = 0.0f;

    if(navGrid) {
        const Vec3 clamped=navGrid->clampWorld(position);
        position.x=clamped.x;position.z=clamped.z;
    }else{
        position.x=std::clamp(position.x,-mapBoundary,mapBoundary);
        position.z=std::clamp(position.z,-mapBoundary,mapBoundary);
    }

    if (state == UnitState::ATTACKING && targetUnitId != -1) {
        Unit* target = findUnitById(allUnits,targetUnitId,spatial);
        if (target && target->isAlive()) {
            Vec3 toEnemy = target->position - position;
            if (toEnemy.lengthXZ() > 0.01f) {
                smoothRotateToward(std::atan2(toEnemy.x, toEnemy.z), deltaTime);
            }
        }
    } else if (currentVelocity.lengthXZ() > 0.25f) {
        smoothRotateToward(std::atan2(currentVelocity.x, currentVelocity.z), deltaTime);
    }
}

bool Unit::intersectRay(const Ray& ray, float& outDistance) const {
    if (!isAlive()) return false;
    Vec3 center = position + Vec3(0.0f, height * 0.5f, 0.0f);
    float pickRadius = radius * 1.35f;

    Vec3 oc = ray.origin - center;
    float b = Vec3::dot(oc, ray.direction);
    float c = Vec3::dot(oc, oc) - pickRadius * pickRadius;
    float discriminant = b * b - c;

    if (discriminant < 0.0f) return false;

    float t = -b - std::sqrt(discriminant);
    if (t < 0.0f) t = -b + std::sqrt(discriminant);
    if (t < 0.0f) return false;

    outDistance = t;
    return true;
}

void Unit::smoothRotateToward(float targetYaw, float deltaTime) {
    float diff = targetYaw - facingYawRad;
    while (diff > PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    facingYawRad += diff * std::min(1.0f, 12.0f * deltaTime);
}

Unit* Unit::findUnitById(std::vector<Unit>& allUnits,int searchId,
                         const UnitSpatialIndex* spatial) {
    if(spatial)return spatial->byId(allUnits,searchId);
    for (auto& u : allUnits) {
        if (u.id == searchId) return &u;
    }
    return nullptr;
}

int Unit::findNearestEnemyId(const std::vector<Unit>& allUnits,float searchRadius,
                             const UnitSpatialIndex* spatial) const {
    int bestId=-1;
    float bestDist=searchRadius;
    const auto consider=[&](const Unit& other) {
        if(other.id==id || !other.isAlive() || other.ownerId==ownerId)return;
        const float distance=(other.position-position).lengthXZ();
        if(distance<=bestDist){bestDist=distance;bestId=other.id;}
    };
    if(spatial){
        for(auto i:spatial->nearby(position,searchRadius))consider(allUnits[i]);
    }else{
        for(const auto& other:allUnits)consider(other);
    }
    return bestId;
}

void Unit::updateCombatStateMachine(float deltaTime,std::vector<Unit>& allUnits,
                                    const UnitSpatialIndex* spatial,
                                    std::vector<CombatHit>* pendingHits) {
    (void)deltaTime;

    switch (state) {
        case UnitState::IDLE: {
            navAgent.stop();
            // Workers follow gathering jobs unless explicitly ordered to fight.
            if (isWorker) break;
            if(holdPositionEnabled) {
                // Do not chase even an enemy detected within the normal aggro
                // radius. Only shoot targets in the weapon's actual range.
                const int candidate=findNearestEnemyId(allUnits,
                    data.attack_range+2.5f,spatial);
                Unit* enemy=findUnitById(allUnits,candidate,spatial);
                if(enemy && (enemy->position-position).lengthXZ()<=
                             data.attack_range+enemy->radius) {
                    targetUnitId=candidate;
                    state=UnitState::ATTACKING;
                }
            }else{
                int enemyId = findNearestEnemyId(allUnits,data.aggro_range,spatial);
                if (enemyId != -1) {
                    targetUnitId = enemyId;
                    state = UnitState::CHASING;
                }
            }
            break;
        }

        case UnitState::MOVING: {
            if (attackMoveEnabled) {
                int enemyId = findNearestEnemyId(allUnits,data.aggro_range,spatial);
                if (enemyId != -1) {
                    targetUnitId = enemyId;
                    state = UnitState::CHASING;
                    break;
                }
            }

            if (navAgent.isNavigationFinished(position)) {
                navAgent.stop();
                attackMoveEnabled = false;
                state = UnitState::IDLE;
            }
            break;
        }

        case UnitState::CHASING: {
            Unit* target = findUnitById(allUnits,targetUnitId,spatial);
            if (!target || !target->isAlive()) {
                targetUnitId = -1;
                chaseWaypoints.clear();
                int nextEnemy = findNearestEnemyId(allUnits,data.aggro_range,spatial);
                if (nextEnemy != -1) {
                    targetUnitId = nextEnemy;
                } else {
                    navAgent.stop();
                    state = UnitState::IDLE;
                }
                break;
            }

            float distToEnemy = (target->position - position).lengthXZ();
            float effectiveRange = data.attack_range + target->radius;

            if (distToEnemy <= effectiveRange) {
                navAgent.stop();
                state = UnitState::ATTACKING;
                chaseWaypoints.clear();
            } else if (navGrid) {
                // Replan periodically or when a moving target leaves the current area.
                chaseRepathTimer -= deltaTime;
                const bool moved = (chaseRouteGoal - target->position).lengthXZ() > 2.0f;
                if (chaseWaypoints.empty() || moved || chaseRepathTimer <= 0.0f) {
                    auto path = navGrid->findPath(position, target->position);
                    if (!path.empty()) {
                        chaseWaypoints = std::move(path);
                        chaseWaypointIndex = 0;
                    } else {
                        chaseWaypoints.clear();
                        navAgent.stop();
                    }
                    chaseRouteGoal = target->position;
                    chaseRepathTimer = moved ? 0.5f : 1.0f;
                }
                while (chaseWaypointIndex < chaseWaypoints.size() &&
                       (chaseWaypoints[chaseWaypointIndex] - position).lengthXZ() <= 0.45f)
                    ++chaseWaypointIndex;
                if (chaseWaypointIndex < chaseWaypoints.size())
                    navAgent.setTargetPosition(chaseWaypoints[chaseWaypointIndex]);
                else {
                    // A building occupies blocked cells: approach a walkable boundary
                    // rather than forcing a steering target inside the structure.
                    auto approach = navGrid->findPath(position, target->position);
                    if (!approach.empty()) navAgent.setTargetPosition(approach.back());
                    else navAgent.stop();
                }
            } else {
                navAgent.setTargetPosition(target->position);
            }
            break;
        }

        case UnitState::ATTACKING: {
            Unit* target = findUnitById(allUnits,targetUnitId,spatial);
            if (!target || !target->isAlive()) {
                targetUnitId = -1;
                chaseWaypoints.clear();
                int nextEnemy = findNearestEnemyId(allUnits,data.aggro_range,spatial);
                if (nextEnemy != -1) {
                    targetUnitId = nextEnemy;
                    state = UnitState::CHASING;
                } else {
                    state = UnitState::IDLE;
                }
                break;
            }

            float distToEnemy = (target->position - position).lengthXZ();
            float effectiveRange = data.attack_range + target->radius;

            if (distToEnemy > effectiveRange * 1.12f) {
                if(holdPositionEnabled) {
                    targetUnitId=-1;
                    state=UnitState::IDLE;
                    navAgent.stop();
                }else{
                    state = UnitState::CHASING;
                    navAgent.setTargetPosition(target->position);
                }
                break;
            }

            if (attackCooldownTimer <= 0.0f) {
                attackCooldownTimer = data.attack_cooldown;
                attackFlashTimer = 0.14f;
                lastAttackTargetPos = target->position + Vec3(0.0f, target->height * 0.55f, 0.0f);
                if(pendingHits)pendingHits->push_back({id,target->id,data.damage,instanceName});
                else target->takeDamage(data.damage, instanceName);
            }
            break;
        }
    }
}