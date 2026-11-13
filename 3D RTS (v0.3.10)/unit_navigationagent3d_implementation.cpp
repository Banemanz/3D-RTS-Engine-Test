#include "Unit.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

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

Vec3 NavigationAgent3D::computeSteeringVelocity(const Unit& self, const std::vector<Unit>& allUnits, float /*deltaTime*/) {
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

    for (const auto& other : allUnits) {
        if (other.id == self.id || !other.isAlive()) continue;

        Vec3 away = self.position - other.position;
        away.y = 0.0f;
        float dist = away.length();

        if (dist < separationRadius) {
            if (dist < 1e-4f) {
                float angle = static_cast<float>(self.id * 137.5f) * (PI / 180.0f);
                away = Vec3(std::cos(angle), 0.0f, std::sin(angle));
                dist = 0.01f;
            }

            float strength = (separationRadius - dist) / separationRadius;
            separationForce += away.normalized() * (strength * separationWeight);
            neighborCount++;
        }
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
    attackMoveEnabled = isAttackMove;
    state = UnitState::MOVING;
    navAgent.setTargetPosition(destination);
}

void Unit::attackTarget(int enemyId) {
    targetUnitId = enemyId;
    attackMoveEnabled = false;
    state = UnitState::CHASING;
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

void Unit::update(float deltaTime, std::vector<Unit>& allUnits, float mapBoundary) {
    if (!isAlive()) return;

    if (attackCooldownTimer > 0.0f) {
        attackCooldownTimer = std::max(0.0f, attackCooldownTimer - deltaTime);
    }
    if (attackFlashTimer > 0.0f) {
        attackFlashTimer = std::max(0.0f, attackFlashTimer - deltaTime);
    }

    updateCombatStateMachine(deltaTime, allUnits);

    Vec3 desiredVel = navAgent.computeSteeringVelocity(*this, allUnits, deltaTime);

    float blend = 1.0f - std::exp(-14.0f * deltaTime);
    currentVelocity = currentVelocity + (desiredVel - currentVelocity) * blend;

    position += currentVelocity * deltaTime;
    position.y = 0.0f;

    position.x = std::clamp(position.x, -mapBoundary, mapBoundary);
    position.z = std::clamp(position.z, -mapBoundary, mapBoundary);

    if (state == UnitState::ATTACKING && targetUnitId != -1) {
        Unit* target = findUnitById(allUnits, targetUnitId);
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

Unit* Unit::findUnitById(std::vector<Unit>& allUnits, int searchId) {
    for (auto& u : allUnits) {
        if (u.id == searchId) return &u;
    }
    return nullptr;
}

int Unit::findNearestEnemyId(const std::vector<Unit>& allUnits, float searchRadius) const {
    int bestId = -1;
    float bestDist = searchRadius;

    for (const auto& other : allUnits) {
        if (other.id == id || !other.isAlive()) continue;
        if (other.data.faction == data.faction) continue;

        float dist = (other.position - position).lengthXZ();
        if (dist <= bestDist) {
            bestDist = dist;
            bestId = other.id;
        }
    }
    return bestId;
}

void Unit::updateCombatStateMachine(float deltaTime, std::vector<Unit>& allUnits) {
    (void)deltaTime;

    switch (state) {
        case UnitState::IDLE: {
            navAgent.stop();
            int enemyId = findNearestEnemyId(allUnits, data.aggro_range);
            if (enemyId != -1) {
                targetUnitId = enemyId;
                state = UnitState::CHASING;
            }
            break;
        }

        case UnitState::MOVING: {
            if (attackMoveEnabled) {
                int enemyId = findNearestEnemyId(allUnits, data.aggro_range);
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
            Unit* target = findUnitById(allUnits, targetUnitId);
            if (!target || !target->isAlive()) {
                targetUnitId = -1;
                int nextEnemy = findNearestEnemyId(allUnits, data.aggro_range);
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
            } else {
                navAgent.setTargetPosition(target->position);
            }
            break;
        }

        case UnitState::ATTACKING: {
            Unit* target = findUnitById(allUnits, targetUnitId);
            if (!target || !target->isAlive()) {
                targetUnitId = -1;
                int nextEnemy = findNearestEnemyId(allUnits, data.aggro_range);
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
                state = UnitState::CHASING;
                navAgent.setTargetPosition(target->position);
                break;
            }

            if (attackCooldownTimer <= 0.0f) {
                attackCooldownTimer = data.attack_cooldown;
                attackFlashTimer = 0.14f;
                lastAttackTargetPos = target->position + Vec3(0.0f, target->height * 0.55f, 0.0f);
                target->takeDamage(data.damage, instanceName);
            }
            break;
        }
    }
}