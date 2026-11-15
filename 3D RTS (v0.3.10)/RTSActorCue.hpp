#pragma once
#include "Math3D.hpp"
#include <cstdint>
#include <string>

// Authorable Actor presentation events. Nothing in these definitions
// modifies authoritative gameplay, commands, or random-number state.
// Stable enum ordering also supplies the F7 inspector combo choices.
enum class RTSActorCueEvent { Attack, Launch, Impact, Death, Cast, Status };
enum class RTSActorCueStyle { Pulse, Ring, Beacon };
inline const char* rtsActorCueEventName(RTSActorCueEvent event){
    switch(event){
        case RTSActorCueEvent::Attack:return "Attack";
        case RTSActorCueEvent::Launch:return "Launch";
        case RTSActorCueEvent::Impact:return "Impact";
        case RTSActorCueEvent::Death:return "Death";
        case RTSActorCueEvent::Cast:return "Cast";
        case RTSActorCueEvent::Status:return "Status";
    }
    return "Attack";
}
inline const char* rtsActorCueStyleName(RTSActorCueStyle style){
    switch(style){
        case RTSActorCueStyle::Pulse:return "Pulse";
        case RTSActorCueStyle::Ring:return "Ring";
        case RTSActorCueStyle::Beacon:return "Beacon";
    }
    return "Pulse";
}
inline bool rtsParseActorCueEvent(const std::string& name,
                                  RTSActorCueEvent& out){
    for(int i=0;i<6;++i){
        auto value=static_cast<RTSActorCueEvent>(i);
        if(name==rtsActorCueEventName(value)){out=value;return true;}
    }
    return false;
}
inline bool rtsParseActorCueStyle(const std::string& name,
                                  RTSActorCueStyle& out){
    for(int i=0;i<3;++i){
        auto value=static_cast<RTSActorCueStyle>(i);
        if(name==rtsActorCueStyleName(value)){out=value;return true;}
    }
    return false;
}
struct RTSActorCueDefinition {
    RTSActorCueEvent event=RTSActorCueEvent::Attack;
    RTSActorCueStyle style=RTSActorCueStyle::Pulse;
    Vec3 tint{.20f,.85f,1.f};
    float radius=1.5f;
    std::uint32_t durationTicks=36;
};
