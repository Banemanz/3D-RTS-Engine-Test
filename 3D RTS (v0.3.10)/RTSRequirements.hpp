#pragma once
#include "DataCatalog.hpp"
#include <cstdint>
#include <string>
#include <vector>

// Reusable headless gameplay predicates, never ImGui or renderer state.
// The small closed enum keeps the first version deterministic; unrecognized
// extension fields on catalog objects remain preserved by DataCatalog.
enum class RTSValidatorKind {
    HealthBelowPercent, EnergyAtLeast, HasBehavior, IsStructure,
    OwnerUnitCountAtLeast
};
enum class RTSValidatorSubject { Caster, Target };
struct RTSValidatorDefinition {
    std::string id;
    RTSValidatorKind kind=RTSValidatorKind::HealthBelowPercent;
    RTSValidatorSubject subject=RTSValidatorSubject::Caster;
    float value=50.f;
    std::string behaviorId;
    std::string unitId;
};
RTSValidatorDefinition compileValidator(const DataCatalog& catalog,
                                        const std::string& id);

enum class RTSRequirementKind { Predicate, All, Any, Not };
struct RTSRequirementDefinition {
    std::string id;
    RTSRequirementKind kind=RTSRequirementKind::Predicate;
    RTSValidatorDefinition predicate;
    std::vector<RTSRequirementDefinition> children;
};
RTSRequirementDefinition compileRequirement(const DataCatalog& catalog,
                                            const std::string& id);
// A Target-scoped predicate has no well-defined context for ground casts or
// production, even if wrapped in Not/Any; reject such links at authoring time.
bool rtsRequirementUsesTarget(const RTSRequirementDefinition& node);
const char* rtsValidatorKindName(RTSValidatorKind kind);
const char* rtsValidatorSubjectName(RTSValidatorSubject subject);
const char* rtsRequirementKindName(RTSRequirementKind kind);
