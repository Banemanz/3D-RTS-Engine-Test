#pragma once
#include "RTSProject.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

// Data-driven authored HUD geometry. This is presentation-only: no simulation
// state, resource economy or replay commands depend on these values.
// Both the native renderer and input-hit router consume the SAME profile.
struct RTSHUDProfile {
    float bottomExtra=0.f;       // added to 194px/160px adaptive bottom strip
    float minimapScale=1.f;      // multiplier for tactical minimap dimensions
    float minimapInset=14.f;     // left offset from viewport
    float commandWidth=330.f;    // command grid width
    float commandInset=14.f;     // right offset from viewport
    float resourceTop=22.f;      // reserve first 22px for creator menu
    float resourceHeight=46.f;

    void validate() const {
        auto check=[](float x,float lo,float hi,const char* key){
            if(!std::isfinite(x)||x<lo||x>hi)
                throw std::invalid_argument(std::string("HUD ")+key+" outside valid range");
        };
        check(bottomExtra,-40.f,65.f,"bottom_extra");
        check(minimapScale,.75f,1.3f,"minimap_scale");
        check(minimapInset,8.f,48.f,"minimap_inset");
        check(commandWidth,260.f,390.f,"command_width");
        check(commandInset,8.f,48.f,"command_inset");
        check(resourceTop,20.f,70.f,"resource_top");
        check(resourceHeight,36.f,72.f,"resource_height");
    }
    bool isDefault()const{
        return bottomExtra==0.f && minimapScale==1.f &&
            minimapInset==14.f && commandWidth==330.f &&
            commandInset==14.f && resourceTop==22.f &&
            resourceHeight==46.f;
    }
    picojson::value toJson() const {
        validate();
        return picojson::value(picojson::object{
            {"bottom_extra",picojson::value(double(bottomExtra))},
            {"minimap_scale",picojson::value(double(minimapScale))},
            {"minimap_inset",picojson::value(double(minimapInset))},
            {"command_width",picojson::value(double(commandWidth))},
            {"command_inset",picojson::value(double(commandInset))},
            {"resource_top",picojson::value(double(resourceTop))},
            {"resource_height",picojson::value(double(resourceHeight))}
        });
    }
    static RTSHUDProfile fromProject(const RTSProject& project){
        RTSHUDProfile result;
        const auto it=project.metadata.find("hud_profile");
        if(it==project.metadata.end())return result; // legacy exact layout
        if(!it->second.is<picojson::object>())
            throw std::invalid_argument("hud_profile must be a JSON object");
        const auto& fields=it->second.get<picojson::object>();
        auto read=[&](const char* name,float& value){
            auto item=fields.find(name);
            if(item==fields.end())return;
            if(!item->second.is<double>())
                throw std::invalid_argument(std::string("hud_profile.")+name+
                                            " must be numeric");
            const double d=item->second.get<double>();
            if(!std::isfinite(d))
                throw std::invalid_argument(std::string("hud_profile.")+name+
                                            " must be finite");
            value=static_cast<float>(d);
        };
        read("bottom_extra",result.bottomExtra);
        read("minimap_scale",result.minimapScale);
        read("minimap_inset",result.minimapInset);
        read("command_width",result.commandWidth);
        read("command_inset",result.commandInset);
        read("resource_top",result.resourceTop);
        read("resource_height",result.resourceHeight);
        result.validate();
        return result;
    }
};
