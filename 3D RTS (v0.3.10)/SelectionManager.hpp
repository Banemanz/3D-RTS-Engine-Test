#pragma once

#include "Math3D.hpp"
#include "RTSCamera.hpp"
#include "Unit.hpp"
#include <vector>

class SelectionManager {
public:
    bool isDragging = false;
    Vec2 dragStartScreen{0.0f, 0.0f};
    Vec2 dragCurrentScreen{0.0f, 0.0f};
    float dragThresholdPixels = 6.0f;

    void onLeftMouseDown(double mouseX, double mouseY);
    void onMouseMove(double mouseX, double mouseY);
    void onLeftMouseUp(double mouseX, double mouseY, bool shiftHeld,
                       const RTSCamera& camera, int screenWidth, int screenHeight,
                       std::vector<Unit>& units);

    Unit* raycastEnemyUnit(double mouseX, double mouseY, const RTSCamera& camera,
                           int screenWidth, int screenHeight, std::vector<Unit>& units);

    void issueAttackOrder(Unit* targetEnemy, std::vector<Unit>& units);
    void issueMoveCommand(const Vec3& groundClickPos, bool isAttackMove, std::vector<Unit>& units);
    int getSelectedCount(const std::vector<Unit>& units) const;

private:
    void clearSelection(std::vector<Unit>& units);
    void performSingleClickSelection(double mouseX, double mouseY, bool shiftHeld,
                                     const RTSCamera& camera, int screenWidth, int screenHeight,
                                     std::vector<Unit>& units);
    void performBoxSelection(bool shiftHeld, const RTSCamera& camera,
                             int screenWidth, int screenHeight, std::vector<Unit>& units);
};