#include "SelectionManager.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

void SelectionManager::onLeftMouseDown(double mouseX, double mouseY) {
    isDragging = true;
    dragStartScreen = Vec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
    dragCurrentScreen = dragStartScreen;
}

void SelectionManager::onMouseMove(double mouseX, double mouseY) {
    if (isDragging) {
        dragCurrentScreen = Vec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
    }
}

void SelectionManager::onLeftMouseUp(double mouseX, double mouseY, bool shiftHeld,
                                     const RTSCamera& camera, int screenWidth, int screenHeight,
                                     std::vector<Unit>& units) {
    if (!isDragging) return;
    dragCurrentScreen = Vec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
    isDragging = false;

    float dragDistance = (dragCurrentScreen - dragStartScreen).length();

    if (dragDistance < dragThresholdPixels) {
        performSingleClickSelection(mouseX, mouseY, shiftHeld, camera, screenWidth, screenHeight, units);
    } else {
        performBoxSelection(shiftHeld, camera, screenWidth, screenHeight, units);
    }
}

Unit* SelectionManager::raycastEnemyUnit(double mouseX, double mouseY, const RTSCamera& camera,
                                         int screenWidth, int screenHeight, std::vector<Unit>& units) {
    Ray clickRay;
    if (!camera.screenPointToRay(mouseX, mouseY, screenWidth, screenHeight, clickRay)) return nullptr;

    Unit* closestEnemy = nullptr;
    float minDist = std::numeric_limits<float>::max();

    for (auto& unit : units) {
        if (!unit.isAlive() || unit.getFaction() == Faction::Humanity) continue;
        float hitDist = 0.0f;
        if (unit.intersectRay(clickRay, hitDist) && hitDist < minDist) {
            minDist = hitDist;
            closestEnemy = &unit;
        }
    }
    return closestEnemy;
}

void SelectionManager::issueAttackOrder(Unit* targetEnemy, std::vector<Unit>& units) {
    if (!targetEnemy || !targetEnemy->isAlive()) return;
    int commanded = 0;
    for (auto& unit : units) {
        if (unit.isAlive() && unit.isSelected && unit.getFaction() == Faction::Humanity) {
            unit.attackTarget(targetEnemy->id);
            commanded++;
        }
    }
    if (commanded > 0) {
        std::cout << "[Attack Order] " << commanded << " Humanity unit(s) targeting "
                  << targetEnemy->instanceName << " (" << targetEnemy->data.resourceName << ")" << std::endl;
    }
}

void SelectionManager::issueMoveCommand(const Vec3& groundClickPos, bool isAttackMove, std::vector<Unit>& units) {
    std::vector<Unit*> selectedUnits;
    for (auto& unit : units) {
        if (unit.isAlive() && unit.isSelected && unit.getFaction() == Faction::Humanity) {
            selectedUnits.push_back(&unit);
        }
    }

    if (selectedUnits.empty()) return;

    std::cout << std::fixed << std::setprecision(2)
              << (isAttackMove ? "[Attack-Move] " : "[Move Order] ")
              << "Commanding " << selectedUnits.size()
              << " unit(s) -> (" << groundClickPos.x << ", "
              << groundClickPos.y << ", " << groundClickPos.z << ")" << std::endl;

    if (selectedUnits.size() == 1) {
        selectedUnits[0]->moveTo(groundClickPos, isAttackMove);
        return;
    }

    const float spacing = 1.45f;
    for (size_t i = 0; i < selectedUnits.size(); ++i) {
        if (i == 0) {
            selectedUnits[i]->moveTo(groundClickPos, isAttackMove);
        } else {
            float angle = static_cast<float>(i) * 2.39996323f;
            float radius = spacing * std::sqrt(static_cast<float>(i));
            Vec3 offset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
            selectedUnits[i]->moveTo(groundClickPos + offset, isAttackMove);
        }
    }
}

int SelectionManager::getSelectedCount(const std::vector<Unit>& units) const {
    int count = 0;
    for (const auto& u : units) if (u.isAlive() && u.isSelected) count++;
    return count;
}

void SelectionManager::clearSelection(std::vector<Unit>& units) {
    for (auto& u : units) u.setSelected(false);
}

void SelectionManager::performSingleClickSelection(double mouseX, double mouseY, bool shiftHeld,
                                                   const RTSCamera& camera, int screenWidth, int screenHeight,
                                                   std::vector<Unit>& units) {
    Ray clickRay;
    if (!camera.screenPointToRay(mouseX, mouseY, screenWidth, screenHeight, clickRay)) return;

    Unit* closestHit = nullptr;
    float minDist = std::numeric_limits<float>::max();

    for (auto& unit : units) {
        if (!unit.isAlive()) continue;
        float hitDist = 0.0f;
        if (unit.intersectRay(clickRay, hitDist) && hitDist < minDist) {
            minDist = hitDist;
            closestHit = &unit;
        }
    }

    if (!shiftHeld) {
        clearSelection(units);
    }

    if (closestHit) {
        if (shiftHeld) {
            closestHit->setSelected(!closestHit->isSelected);
        } else {
            closestHit->setSelected(true);
        }
        std::cout << "[Select] " << closestHit->instanceName
                  << " [" << closestHit->data.resourceName
                  << " | Faction: " << factionToString(closestHit->getFaction())
                  << " | HP: " << static_cast<int>(closestHit->currentHp) << "/" << static_cast<int>(closestHit->data.max_hp)
                  << " | Armor: " << closestHit->data.armor
                  << " | Supply: " << closestHit->data.supply_cost
                  << " | State: " << unitStateToString(closestHit->state) << "]" << std::endl;
    } else if (!shiftHeld) {
        std::cout << "[Select] Cleared selection." << std::endl;
    }
}

void SelectionManager::performBoxSelection(bool shiftHeld, const RTSCamera& camera,
                                           int screenWidth, int screenHeight, std::vector<Unit>& units) {
    float minX = std::min(dragStartScreen.x, dragCurrentScreen.x);
    float maxX = std::max(dragStartScreen.x, dragCurrentScreen.x);
    float minY = std::min(dragStartScreen.y, dragCurrentScreen.y);
    float maxY = std::max(dragStartScreen.y, dragCurrentScreen.y);

    if (!shiftHeld) {
        clearSelection(units);
    }

    int newlySelected = 0;
    for (auto& unit : units) {
        if (!unit.isAlive() || unit.getFaction() != Faction::Humanity) continue;
        Vec3 unitCenter = unit.position + Vec3(0.0f, unit.height * 0.45f, 0.0f);
        Vec2 screenPos;

        if (camera.worldToScreen(unitCenter, screenWidth, screenHeight, screenPos)) {
            if (screenPos.x >= minX && screenPos.x <= maxX &&
                screenPos.y >= minY && screenPos.y <= maxY) {
                unit.setSelected(true);
                newlySelected++;
            }
        }
    }

    std::cout << "[Box Select] Captured " << newlySelected
              << " Humanity unit(s) | Total Selected: " << getSelectedCount(units) << std::endl;
}