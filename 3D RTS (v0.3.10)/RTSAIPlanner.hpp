#pragma once
#include "RTSCommandStream.hpp"
#include "Unit.hpp"
#include <cstdint>
#include <vector>

// Headless, read-only combat policy. These are deterministic proposals,
// not immediate unit state changes. The simulation queues/validates them.
struct RTSAICombatPolicy {
    bool enabled=true;
    float defenseRadius=14.0f; // 0 = strategic objective only
};
std::vector<RTSCommand> planRTSEnemyAttackOrders(
    const std::vector<Unit>& units,int aiOwner,int homeBaseId,
    int strategicObjectiveId,std::uint64_t executionTick,
    const RTSAICombatPolicy& policy={});
