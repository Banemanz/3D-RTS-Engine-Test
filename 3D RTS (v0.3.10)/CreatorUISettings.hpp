#pragma once
#include "third_party/picojson.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

// Preferences belong to the person running the creator, NOT the RTS map.
// Keep them separate from simulation data, state hashes and replay tapes.
struct CreatorUISettings {
    double uiScale=1.15;       // readable default; 0.85 to 2.50
    bool followSystemDpi=true;
    bool enhancedContrast=false;
    bool separateModules=true; // one independently movable OS/dockable module
    int schema=1;

    static constexpr double MinScale=0.85;
    static constexpr double MaxScale=2.50;
    static double effective(double manual,double monitorScale,bool follow){
        if(!std::isfinite(manual) || manual<MinScale || manual>MaxScale)
            throw std::invalid_argument("Creator UI scale must be 85%-250%");
        if(!std::isfinite(monitorScale) || monitorScale<0.5 ||
           monitorScale>8.0)monitorScale=1.0;
        // Allow up to 400% effective scaling (e.g., 200% DPI * 200% UI);
        // clamp extreme/malformed OS values before atlas allocation.
        return std::clamp(manual*(follow?monitorScale:1.0),0.75,4.0);
    }
    double effective(double monitorScale)const{
        return effective(uiScale,monitorScale,followSystemDpi);
    }
    picojson::value toJson()const{
        return picojson::value(picojson::object{
            {"schema",picojson::value(double(schema))},
            {"ui_scale",picojson::value(uiScale)},
            {"follow_system_dpi",picojson::value(followSystemDpi)},
            {"enhanced_contrast",picojson::value(enhancedContrast)},
            {"separate_modules",picojson::value(separateModules)}
        });
    }
    static CreatorUISettings fromJson(const picojson::value& input){
        if(!input.is<picojson::object>())
            throw std::invalid_argument("Creator UI preferences must be a JSON object");
        const auto& o=input.get<picojson::object>();
        CreatorUISettings p;
        const auto number=[&](const char* key,double fallback){
            auto it=o.find(key);
            if(it==o.end())return fallback;
            if(!it->second.is<double>())
                throw std::invalid_argument(std::string("Invalid numeric setting: ")+key);
            return it->second.get<double>();
        };
        const auto flag=[&](const char* key,bool fallback){
            auto it=o.find(key);
            if(it==o.end())return fallback;
            if(!it->second.is<bool>())
                throw std::invalid_argument(std::string("Invalid toggle: ")+key);
            return it->second.get<bool>();
        };
        if(number("schema",1.)!=1.)
            throw std::invalid_argument("Unsupported creator UI preferences version");
        p.uiScale=number("ui_scale",p.uiScale);
        p.followSystemDpi=flag("follow_system_dpi",p.followSystemDpi);
        p.enhancedContrast=flag("enhanced_contrast",p.enhancedContrast);
        p.separateModules=flag("separate_modules",p.separateModules);
        (void)p.effective(1.);
        return p;
    }
    static std::filesystem::path userConfigPath(){
#ifdef _WIN32
        const char* appData=std::getenv("LOCALAPPDATA");
        if(appData && *appData)
            return std::filesystem::path(appData)/"RTSCreator"/"ui_settings.json";
#endif
        const char* xdg=std::getenv("XDG_CONFIG_HOME");
        if(xdg && *xdg)
            return std::filesystem::path(xdg)/"rts-creator"/"ui_settings.json";
        const char* home=std::getenv("HOME");
        if(home && *home)
            return std::filesystem::path(home)/".config"/"rts-creator"/"ui_settings.json";
        return {}; // no home directory -> session-only settings
    }
    static CreatorUISettings load(const std::filesystem::path& path){
        if(path.empty())return {};
        std::ifstream in(path,std::ios::binary|std::ios::ate);
        if(!in)return {};
        auto bytes=in.tellg();
        if(bytes<0 || bytes>8192)return {};
        in.seekg(0);
        std::string json(std::istreambuf_iterator<char>{in},
                         std::istreambuf_iterator<char>{});
        picojson::value root;
        if(!picojson::parse(root,json).empty())return {};
        try{return fromJson(root);}
        catch(const std::exception&){return {};} // malformed preferences never crash the game
    }
    bool save(const std::filesystem::path& path)const{
        if(path.empty())return false;
        try{
            (void)effective(1.);
            std::filesystem::create_directories(path.parent_path());
            const std::filesystem::path tmp=path.string()+".tmp";
            {
                std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
                if(!out)return false;
                out<<toJson().serialize()<<"\n";
                if(!out)return false;
            }
            std::error_code ec;
            std::filesystem::rename(tmp,path,ec);
            if(ec){
                std::filesystem::copy_file(tmp,path,
                    std::filesystem::copy_options::overwrite_existing,ec);
                std::filesystem::remove(tmp);
            }
            return !ec;
        }catch(const std::exception&){return false;}
    }
};
inline const char* creatorModuleTitle(int panel){
    switch(panel){
        case 0:return "RTS Creator | Playtest";
        case 1:return "RTS Creator | Terrain Editor";
        case 2:return "RTS Creator | Data Editor";
        case 3:return "RTS Creator | Trigger Editor";
        case 4:return "RTS Creator | Scene Editor";
        default:return "RTS Creator | Workspace";
    }
}
