#pragma once

#include "Math3D.hpp"
#include <string>

enum class Faction {
    Humanity,
    Hybrid
};

inline std::string factionToString(Faction f) {
    return (f == Faction::Humanity) ? "Humanity" : "Hybrid";
}

class UnitData {
public:
    std::string resourceName = "UnnamedUnit";
    Faction faction          = Faction::Humanity;
    float max_hp             = 45.0f;
    float armor              = 0.0f;
    float damage             = 6.0f;
    float attack_range       = 6.0f;
    float attack_cooldown    = 0.85f;
    float aggro_range        = 11.0f;
    float sight_range        = 14.0f;
    float move_speed         = 9.5f;
    int supply_cost          = 1;
    int mineral_cost         = 50;
    float build_time         = 18.0f;
    float visual_scale       = 1.0f;
    Vec3 faction_color       = Vec3(0.22f, 0.56f, 0.95f);

    static UnitData fromTresString(const std::string& tresContent);
    std::string toTresString() const;
};

extern const char* HUMAN_MARINE_TRES;
extern const char* HYBRID_ZEALOT_TRES;