#include "CreatorUISettings.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Creator UI settings checks must retain assertions in Release"
#endif
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
bool invalid(const picojson::value& value){
    try{(void)CreatorUISettings::fromJson(value);}
    catch(const std::invalid_argument&){return true;}
    return false;
}
}
int main(){
    CreatorUISettings base;
    assert(base.separateModules && base.followSystemDpi);
    assert(!base.enhancedContrast);
    assert(std::fabs(base.uiScale-1.15)<.001);
    assert(std::fabs(base.effective(1.)-1.15)<.001);
    assert(std::fabs(base.effective(1.5)-1.725)<.001);
    base.followSystemDpi=false;
    assert(std::fabs(base.effective(2.5)-1.15)<.001);
    base.uiScale=2.5;
    base.followSystemDpi=true;
    assert(base.effective(2.5)==4.); // accessibility 400% safety cap
    assert(base.effective(std::numeric_limits<double>::quiet_NaN())==2.5);
    base.enhancedContrast=true;
    base.separateModules=false;
    const auto source=base.toJson();
    assert(source.is<picojson::object>());
    const auto round=CreatorUISettings::fromJson(source);
    assert(round.uiScale==2.5 && round.followSystemDpi);
    assert(round.enhancedContrast && !round.separateModules);
    auto changed=source.get<picojson::object>();
    changed["ui_scale"]=picojson::value(0.5);
    assert(invalid(picojson::value(changed)));
    changed["ui_scale"]=picojson::value(3.);
    assert(invalid(picojson::value(changed)));
    changed["ui_scale"]=picojson::value("giant");
    assert(invalid(picojson::value(changed)));
    changed=source.get<picojson::object>();
    changed["schema"]=picojson::value(2.);
    assert(invalid(picojson::value(changed)));
    changed=source.get<picojson::object>();
    changed["enhanced_contrast"]=picojson::value(3.);
    assert(invalid(picojson::value(changed)));
    changed=source.get<picojson::object>();
    changed["future_option"]=picojson::value("kept external");
    assert(CreatorUISettings::fromJson(picojson::value(changed)).uiScale==2.5);
    assert(std::string(creatorModuleTitle(1))=="RTS Creator | Terrain Editor");
    assert(std::string(creatorModuleTitle(2))=="RTS Creator | Data Editor");
    assert(std::string(creatorModuleTitle(3))=="RTS Creator | Trigger Editor");
    assert(std::string(creatorModuleTitle(4))=="RTS Creator | Scene Editor");

    // User appearance preferences are independent of map files and replay
    // metadata, and corrupt/oversized saved files must NEVER abort startup.
    const auto dir=std::filesystem::current_path()/"creator-ui-test";
    const auto path=dir/"ui_settings.json";
    std::filesystem::remove_all(dir);
    assert(round.save(path));
    auto loaded=CreatorUISettings::load(path);
    assert(loaded.uiScale==2.5 && loaded.enhancedContrast);
    loaded.uiScale=1.75;
    assert(loaded.save(path)); // overwrite existing preference file (Windows too)
    assert(CreatorUISettings::load(path).uiScale==1.75);
    {
        std::ofstream out(path,std::ios::trunc);
        out<<"{malformed json";
    }
    assert(std::fabs(CreatorUISettings::load(path).uiScale-1.15)<.001);
    {
        std::ofstream out(path,std::ios::trunc);
        out<<std::string(10000,'x');
    }
    assert(std::fabs(CreatorUISettings::load(path).uiScale-1.15)<.001);
    std::filesystem::remove_all(dir);
    std::cout<<"Independent module titles, bounded scaling, OS-DPI math, "
                "contrast, user preferences, corrupt-file safety passed\n";
}
