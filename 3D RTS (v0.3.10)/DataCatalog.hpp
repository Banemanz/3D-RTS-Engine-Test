#pragma once
#include "UnitData.hpp"
#include "RTSAreaShape.hpp"
#include "RTSActorCue.hpp"
#include "third_party/picojson.h"
#include <stdexcept>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// A catalog is an extensible typed object graph. Fields deliberately retain
// arbitrary JSON values, including future mod-defined fields not yet understood
// by the game runtime.
struct CatalogEntry {
    std::string id;
    std::string type;
    std::string parent;
    picojson::object fields;
};

class DataCatalog {
public:
    const std::vector<CatalogEntry>& entries() const { return entries_; }
    const CatalogEntry* find(const std::string& id) const;
    CatalogEntry* find(const std::string& id);
    bool add(CatalogEntry entry,std::string* error=nullptr);
    bool cloneDerived(const std::string& original,const std::string& newId);
    bool rename(const std::string& oldId,const std::string& newId);
    // Low-level erase; project-layer dependency checks protect actual maps.
    bool erase(const std::string& id);
    bool setField(const std::string& id,const std::string& key,
                  const picojson::value& value);
    picojson::object resolvedFields(const std::string& id) const;
    void validate() const;
    picojson::value serialize() const;
    static DataCatalog deserialize(const picojson::value& root);
    static DataCatalog defaults();
private:
    std::vector<CatalogEntry> entries_;
    // Stable indices rather than pointers, so vector reallocation and copies
    // cannot invalidate ID lookups. Only add(), rename() and erase() can change entry identities.
    std::unordered_map<std::string,std::size_t> idIndex_;
    void resolveInto(const std::string& id,picojson::object& out,
                     std::vector<std::string>& stack) const;
};

// Bounded simulation effects are stored in the same editable catalog graph.
// Gameplay logic can compile them without knowing anything about OpenGL/UI.
enum class EffectOperationKind { Damage, Heal, PeriodicDamage, PeriodicHeal,
                                 ApplyBehavior, RemoveBehavior };
struct EffectOperation {
    EffectOperationKind kind=EffectOperationKind::Damage;
    float amount=0.f;
    std::string behaviorId;
    std::uint32_t intervalTicks=0;
    std::uint32_t durationTicks=0;
};
// Reusable status modifier, independent of the Unit and the source Ability.
struct BehaviorDefinition {
    std::string id;
    std::uint32_t durationTicks=180;
    int maxStacks=1;
    float armorBonus=0.f;
    float damageBonus=0.f;
    float speedMultiplier=1.f;
    bool refreshDuration=true;
    bool dispellable=true;
};
BehaviorDefinition compileBehavior(const DataCatalog& catalog,
                                   const std::string& id);
std::vector<EffectOperation> compileEffect(const DataCatalog& catalog,
                                            const std::string& id);

// Reusable gameplay projectile data; no asset- or OS-specific code here.
struct ProjectileDefinition {
    std::string id;
    float speed=12.f;             // world units/sec, tick quantized
    float arcHeight=1.f;          // vertical parabola at midpoint
    float visualScale=.3f;
    float collisionRadius=.35f;
    bool collideUnits=false;     // swept 3D contact triggers early impact
    Vec3 tint{1.f,.6f,.2f};     // procedural actor tint; authored, not sprites
};
ProjectileDefinition compileProjectile(const DataCatalog& catalog,
                                       const std::string& id);

// First-class reusable standard attack profiles. A Unit without a 'weapon'
// reference retains its legacy damage/range/cooldown entirely unchanged.
struct WeaponDefinition {
    std::string id;
    float damage=7.f;
    float range=6.f;
    float cooldown=.75f;     // seconds, same as legacy attack cooldown
    float armorPiercing=0.f; // subtracts from target's nonnegative armor
    std::string projectileId; // typed Projectile delivery; empty retains hitscan
};
WeaponDefinition compileWeapon(const DataCatalog& catalog,
                               const std::string& id);

// Abilities bind an authored Effect graph to a Unit command. Only the
// data objects are stored in the map; cooldown clocks live in simulation.
enum class AbilityTarget { Self, Ally, Enemy, Any, Ground };
// Ground-targeted area Effects use a separate team filter. Target selection is
// resolved on the simulation tick, never from renderer selection pointers.
enum class AbilityAreaTeam { Enemy, Ally, Any };
struct AbilityDefinition {
    std::string id;
    std::string effect;
    AbilityTarget target=AbilityTarget::Self;
    std::uint32_t cooldownTicks=60;
    float castRange=0.f;
    int mineralCost=0;
    int energyCost=0;
    float areaRadius=0.f; // Ground only: 0.5..24 world units
    RTSAreaShape areaShape=RTSAreaShape::Circle;
    float areaAngle=90.f; // Cone opening in degrees (10..170)
    float areaWidth=2.f;  // Line width (.25..12)
    AbilityAreaTeam areaTeam=AbilityAreaTeam::Enemy;
    std::uint32_t impactDelayTicks=0; // Ground only: 0..300 ticks at 60Hz
    std::string projectileId; // optional typed Projectile reference for Ground
};
AbilityDefinition compileAbility(const DataCatalog& catalog,
                                 const std::string& id);
std::vector<std::string> unitAbilityIds(const DataCatalog& catalog,
                                         const std::string& unitId);

// Typed Actor event rules are strictly bounded and presentation-only.
std::vector<RTSActorCueDefinition> compileActorCues(
    const DataCatalog& catalog,const std::string& actorId);

// Typed runtime adapters. Unknown catalog fields are not deleted or discarded.
UnitData unitFromCatalog(const DataCatalog& catalog,const std::string& id);
struct ActorVisual {
    Vec3 tint{1.f,1.f,1.f};
    Vec3 scale{1.f,1.f,1.f};
    float opacity=1.f;
    std::string mesh="procedural.unit";
    std::string texture;
    std::string animation;
    std::string sound;
    std::vector<RTSActorCueDefinition> cues;
};
ActorVisual actorFromCatalog(const DataCatalog& catalog,const std::string& actorId);
