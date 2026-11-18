#pragma once
#include "RTSProject.hpp"

// Resolves inherited catalog data plus a placement's unit/actor/model overrides.
// Used by project validation, the editor preview and skirmish startup.
struct ResolvedUnitInstance {
    UnitData unit;
    ActorVisual actor;
    bool hasActor = false;
    bool isStructure = false;
    bool isWorker = false;
};
ResolvedUnitInstance resolvePlacedUnit(const RTSProject& project,
                                       const ProjectPlacement& placement);
ActorVisual resolvePlacedDoodad(const RTSProject& project,
                                const ProjectPlacement& placement);
