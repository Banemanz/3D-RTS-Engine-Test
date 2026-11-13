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
    const auto marine=UnitData::fromTresString(HUMAN_MARINE_TRES);
    const auto zealot=UnitData::fromTresString(HYBRID_ZEALOT_TRES);
    assert(marine.max_hp==45 && marine.faction==Faction::Humanity);
    assert(zealot.max_hp==160 && zealot.faction==Faction::Hybrid);
    assert(marine.toTresString().find("resource_name = ")!=std::string::npos);
    assert(invalid("[resource]\nmax_hp = 0\n"));
    assert(invalid("[resource]\nattack_cooldown = -1\n"));
    assert(invalid("[resource]\nmax_hp = not_a_number\n"));
    assert(invalid("[resource]\nmax_hp = 10hello\n"));
    assert(invalid("[resource]\nfaction = Other\n"));
    assert(invalid("[resource]\nsupply_cost = 2.5\n"));
    std::cout<<"unitdata: validation tests passed\n";
}
