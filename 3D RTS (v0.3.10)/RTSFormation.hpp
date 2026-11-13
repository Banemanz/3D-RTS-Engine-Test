#pragma once
#include "Math3D.hpp"
#include <vector>

// Deterministic O(N log N) loose group formation assignments.
// Separate from navigation so editors/abilities can reuse layouts and
// movement planners remain responsible for walkable destination snapping.
struct RTSFormationMember {
    int id=0;
    Vec3 position{};
    float radius=.6f;
};
struct RTSFormationSlot {
    int id=0;
    Vec3 destination{};
};

std::vector<RTSFormationSlot> arrangeRTSFormation(
    const std::vector<RTSFormationMember>& members,
    const Vec3& desiredCenter,float spacingMultiplier=2.4f);
