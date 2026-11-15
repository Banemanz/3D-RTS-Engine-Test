#pragma once
#include "DataCatalog.hpp"
#include <cstdint>
#include <string>
#include <vector>

// Authored per-player research, distinct from ephemeral Actor/Behavior state.
// Existing and future matching Units receive the same unlocked modifiers.
struct RTSUpgradeDefinition {
    std::string id;
    std::string unitId;         // empty means all Unit catalog types
    std::string requirementId;  // Caster-only prerequisite
    int maxLevel=1;             // 1..5
    int mineralCost=100;        // per level
    std::uint32_t researchTicks=180;
    float damagePerLevel=0.f;   // normal attacks only
    float armorPerLevel=0.f;    // all physical damage
};
RTSUpgradeDefinition compileUpgrade(const DataCatalog& catalog,
                                    const std::string& id);
// Validates optional ordered per-producer researchable_upgrades array.
std::vector<std::string> unitResearchIds(const DataCatalog& catalog,
                                         const std::string& unitId);
