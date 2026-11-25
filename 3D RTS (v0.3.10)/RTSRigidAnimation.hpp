#pragma once
// Rigid-part animation of baked RTSMESH3 geometry. No authoritative state.
#include "RTSModelAttachment.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

struct RTSRigidKey {
    float time=0;
    Vec3 offset{},rotation{},scale{1,1,1}; // Euler degrees, local XYZ
};
struct RTSRigidTrack {
    std::string node;
    std::vector<RTSRigidKey> keys;
};
struct RTSRigidClip {
    float duration=2;
    bool loop=true;
    std::vector<RTSRigidTrack> tracks;
};
inline float rtsClipTime(const RTSRigidClip& clip,double seconds){
    if(!std::isfinite(seconds) || !std::isfinite(clip.duration) || clip.duration<=0)return 0;
    if(!clip.loop)return static_cast<float>(std::clamp(seconds,0.0,double(clip.duration)));
    const double t=std::fmod(seconds,double(clip.duration));
    return static_cast<float>(t<0?t+clip.duration:t);
}
inline RTSRigidKey rtsSampleRigidTrack(const RTSRigidTrack& track,float time){
    if(track.keys.empty())return {};
    if(time<=track.keys.front().time)return track.keys.front();
    if(time>=track.keys.back().time)return track.keys.back();
    const auto hi=std::upper_bound(track.keys.begin(),track.keys.end(),time,
        [](float t,const RTSRigidKey& key){return t<key.time;});
    const auto& a=*(hi-1);const auto& b=*hi;
    const float t=(time-a.time)/(b.time-a.time);
    // Explicit Euler interpolation preserves authored rotations >180 degrees,
    // including a full 360 degree loop; no implicit shortest arc.
    return {time,a.offset+(b.offset-a.offset)*t,a.rotation+(b.rotation-a.rotation)*t,
            a.scale+(b.scale-a.scale)*t};
}
inline Mat4 rtsRigidDelta(const RTSRigidKey& k){
    const float x=toRadians(k.rotation.x),z=toRadians(k.rotation.z);
    Mat4 rx,rz;
    rx.m[5]=std::cos(x);rx.m[9]=-std::sin(x);rx.m[6]=std::sin(x);rx.m[10]=std::cos(x);
    rz.m[0]=std::cos(z);rz.m[4]=-std::sin(z);rz.m[1]=std::sin(z);rz.m[5]=std::cos(z);
    return Mat4::translate(k.offset)*rz*Mat4::rotateY(toRadians(k.rotation.y))*rx*Mat4::scale(k.scale);
}
inline Mat4 rtsNodeMatrix(const std::array<float,16>& a){
    Mat4 m;std::copy(a.begin(),a.end(),m.m);return m;
}
// Double-precision affine inverse. Mat4::inverse's small determinant fallback
// is unsuitable for tiny but valid imported bind transforms.
inline std::optional<Mat4> rtsInverseBind(const Mat4& m){
    const double a=m.m[0],b=m.m[4],c=m.m[8],d=m.m[1],e=m.m[5],f=m.m[9],
                 g=m.m[2],h=m.m[6],i=m.m[10];
    const double det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(det)||det<=0)return std::nullopt;
    const double v[9]={e*i-f*h,f*g-d*i,d*h-e*g,c*h-b*i,a*i-c*g,b*g-a*h,b*f-c*e,c*d-a*f,a*e-b*d};
    Mat4 out;
    for(int col=0;col<3;++col)for(int row=0;row<3;++row)
        out.m[col*4+row]=static_cast<float>(v[col*3+row]/det);
    for(int row=0;row<3;++row)
        out.m[12+row]=static_cast<float>(-(double(out.m[row])*m.m[12]+
            double(out.m[4+row])*m.m[13]+double(out.m[8+row])*m.m[14]));
    for(float f0:out.m)if(!std::isfinite(f0))return std::nullopt;
    return out;
}
struct RTSRigidPose {
    std::array<Mat4,128> model;
    std::array<Mat4,32> surfaceDelta;
    bool valid=false;
};
// Precompute once per cached GPU asset; pose sampling does not allocate.
struct RTSRigidRig {
    std::array<Mat4,128> inverseBind;
    bool valid=false;
    explicit RTSRigidRig(const std::vector<RTSModelNode>& nodes){
        if(nodes.empty()||nodes.size()>128)return;
        for(std::size_t j=0;j<nodes.size();++j){
            const auto inverse=rtsInverseBind(rtsNodeMatrix(nodes[j].model));
            if(!inverse)return;
            inverseBind[j]=*inverse;
        }
        valid=true;
    }
};
inline RTSRigidPose rtsSampleRigidPose(const std::vector<RTSModelNode>& nodes,
                                     const RTSRigidRig& rig,const RTSRigidClip& clip,double seconds){
    RTSRigidPose pose;
    if(!rig.valid||nodes.empty()||nodes.size()>128||clip.tracks.empty())return pose;
    std::array<const RTSRigidTrack*,128> tracks{};
    for(const auto& track:clip.tracks){
        const auto j=rtsFindUniqueModelNode(nodes,track.node);
        if(!j || tracks[*j])return pose;
        tracks[*j]=&track;
    }
    const float time=rtsClipTime(clip,seconds);
    std::array<bool,128> changed{};
    for(std::size_t j=0;j<nodes.size();++j){
        const auto& n=nodes[j];
        if(n.parent!=RTS_MODEL_ROOT && n.parent>=j)return pose;
        if(n.firstSurface>32 || n.surfaceCount>32-n.firstSurface)return pose;
        changed[j]=tracks[j] || (n.parent!=RTS_MODEL_ROOT && changed[n.parent]);
        if(!changed[j]){
            pose.model[j]=rtsNodeMatrix(n.model);
            continue; // exact untouched bind geometry; no numerical drift
        }
        auto local=rtsNodeMatrix(n.local);
        if(tracks[j])local=local*rtsRigidDelta(rtsSampleRigidTrack(*tracks[j],time));
        pose.model[j]=n.parent==RTS_MODEL_ROOT?local:pose.model[n.parent]*local;
        const Mat4 delta=pose.model[j]*rig.inverseBind[j];
        for(float f:pose.model[j].m)if(!std::isfinite(f)||std::abs(f)>1e9f)return RTSRigidPose{};
        for(float f:delta.m)if(!std::isfinite(f)||std::abs(f)>1e9f)return RTSRigidPose{};
        for(std::uint32_t s=0;s<n.surfaceCount;++s)pose.surfaceDelta[n.firstSurface+s]=delta;
    }
    pose.valid=true;return pose;
}
inline std::optional<Mat4> rtsPosedAttachment(const std::vector<RTSModelNode>& nodes,
    const RTSRigidPose* pose,const Mat4& parent,const RTSModelAttachment& a){
    if(!pose||!pose->valid)return rtsAttachmentTransform(nodes,parent,a);
    const auto index=rtsFindUniqueModelNode(nodes,a.socket);
    if(!index)return std::nullopt;
    return parent*pose->model[*index]*Mat4::translate(a.offset)*
        Mat4::rotateY(toRadians(a.yaw))*
        Mat4::scale(Vec3(a.scale.x*a.modelScale.x,a.scale.y*a.modelScale.y,a.scale.z*a.modelScale.z));
}
