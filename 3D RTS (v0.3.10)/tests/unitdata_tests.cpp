#include "UnitData.hpp"
#include <cassert>
#include <stdexcept>
#include <iostream>

static bool invalid(const std::string& data) {
    try { (void)UnitData::fromTresString(data); }
    catch(const std::invalid_argument&) {return true;}
    catch(const std::out_of_range&) {return true;}
    return false;
}
int main() {
    const auto ranger=UnitData::fromTresString(HUMAN_RANGER_TRES);
    const auto rift_raider=UnitData::fromTresString(HYBRID_RAIDER_TRES);
    assert(ranger.max_hp==45 && ranger.faction==Faction::Humanity);
    assert(rift_raider.max_hp==160 && rift_raider.faction==Faction::Hybrid);
    assert(ranger.toTresString().find("resource_name = ")!=std::string::npos);
    assert(invalid("[resource]\nmax_hp = 0\n"));
    assert(invalid("[resource]\nattack_cooldown = -1\n"));
    assert(invalid("[resource]\nmax_hp = not_a_number\n"));
    assert(invalid("[resource]\nmax_hp = 10hello\n"));
    assert(invalid("[resource]\nfaction = Other\n"));
    assert(invalid("[resource]\nsupply_cost = 2.5\n"));
    std::cout<<"unitdata: validation tests passed\n";
}
