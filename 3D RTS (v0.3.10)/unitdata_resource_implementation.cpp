#include "UnitData.hpp"
#include <sstream>
#include <cmath>
#include <stdexcept>

const char* HUMAN_MARINE_TRES = R"(
[gd_resource type="UnitData" format=3]

[resource]
resource_name = "Human Marine"
faction = "Humanity"
max_hp = 45.0
armor = 0.0
damage = 7.0
attack_range = 6.5
attack_cooldown = 0.75
aggro_range = 10.5
move_speed = 9.5
supply_cost = 1
mineral_cost = 50
build_time = 15.0
visual_scale = 0.92
)";

const char* HYBRID_ZEALOT_TRES = R"(
[gd_resource type="UnitData" format=3]

[resource]
resource_name = "Hybrid Zealot"
faction = "Hybrid"
max_hp = 160.0
armor = 3.0
damage = 18.0
attack_range = 1.85
attack_cooldown = 1.05
aggro_range = 11.0
move_speed = 8.2
supply_cost = 3
mineral_cost = 150
build_time = 32.0
visual_scale = 1.28
)";

UnitData UnitData::fromTresString(const std::string& tresContent) {
    UnitData data;
    std::istringstream stream(tresContent);
    std::string line;

    auto trim = [](std::string s) {
        size_t first = s.find_first_not_of(" \t\r\n\"");
        if (first == std::string::npos) return std::string("");
        size_t last = s.find_last_not_of(" \t\r\n\"");
        return s.substr(first, last - first + 1);
    };

    auto numericFloat = [](const std::string& s,const std::string& field) {
        size_t parsed=0;
        float n=std::stof(s,&parsed);
        if(parsed!=s.size() || !std::isfinite(n))
            throw std::invalid_argument("Invalid floating point value for "+field+": "+s);
        return n;
    };
    auto numericInt = [](const std::string& s,const std::string& field) {
        size_t parsed=0;
        int n=std::stoi(s,&parsed);
        if(parsed!=s.size())
            throw std::invalid_argument("Invalid integer value for "+field+": "+s);
        return n;
    };

    while (std::getline(stream, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "resource_name")       data.resourceName    = val;
        else if (key == "faction") {
            if(val == "Hybrid")data.faction = Faction::Hybrid;
            else if(val == "Humanity")data.faction = Faction::Humanity;
            else throw std::invalid_argument("Unknown unit faction: "+val);
        }
        else if (key == "max_hp")         data.max_hp          = numericFloat(val,key);
        else if (key == "armor")          data.armor           = numericFloat(val,key);
        else if (key == "damage")         data.damage          = numericFloat(val,key);
        else if (key == "attack_range")   data.attack_range    = numericFloat(val,key);
        else if (key == "attack_cooldown")data.attack_cooldown = numericFloat(val,key);
        else if (key == "aggro_range")    data.aggro_range     = numericFloat(val,key);
        else if (key == "sight_range")    data.sight_range     = numericFloat(val,key);
        else if (key == "move_speed")     data.move_speed      = numericFloat(val,key);
        else if (key == "supply_cost")    data.supply_cost     = numericInt(val,key);
        else if (key == "mineral_cost")   data.mineral_cost    = numericInt(val,key);
        else if (key == "build_time")     data.build_time      = numericFloat(val,key);
        else if (key == "visual_scale")   data.visual_scale    = numericFloat(val,key);
    }

    if(data.resourceName.empty() || data.max_hp<=0 || data.armor<0 ||
       data.damage<0 || data.attack_range<0 || data.attack_cooldown<=0 ||
       data.aggro_range<0 || data.sight_range<0 || data.sight_range>128 ||
       data.move_speed<0 || data.supply_cost<0 ||
       data.mineral_cost<0 || data.build_time<0 || data.visual_scale<=0) {
        throw std::invalid_argument("UnitData includes an invalid stat or empty resource name");
    }

    if (data.faction == Faction::Humanity) {
        data.faction_color = Vec3(0.22f, 0.58f, 0.96f);
    } else {
        data.faction_color = Vec3(0.76f, 0.24f, 0.88f);
    }
    return data;
}

std::string UnitData::toTresString() const {
    std::ostringstream out;
    out << "[gd_resource type=\"UnitData\" format=3]\n\n"
        << "[resource]\n"
        << "resource_name = \"" << resourceName << "\"\n"
        << "faction = \"" << factionToString(faction) << "\"\n"
        << "max_hp = " << max_hp << "\n"
        << "armor = " << armor << "\n"
        << "damage = " << damage << "\n"
        << "attack_range = " << attack_range << "\n"
        << "attack_cooldown = " << attack_cooldown << "\n"
        << "aggro_range = " << aggro_range << "\n"
        << "sight_range = " << sight_range << "\n"
        << "move_speed = " << move_speed << "\n"
        << "supply_cost = " << supply_cost << "\n"
        << "mineral_cost = " << mineral_cost << "\n"
        << "build_time = " << build_time << "\n"
        << "visual_scale = " << visual_scale << "\n";
    return out.str();
}