#include "RTSFormation.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

std::vector<RTSFormationSlot> arrangeRTSFormation(
    const std::vector<RTSFormationMember>& members,
    const Vec3& desiredCenter,float spacingMultiplier) {
    const std::size_t n=members.size();
    std::vector<RTSFormationSlot> result(n);
    if(n==0)return result;
    if(n==1){
        result[0]={members[0].id,desiredCenter};
        return result;
    }

    Vec3 centroid{0,0,0};
    float maxRadius=.65f;
    for(const auto& member:members) {
        centroid+=member.position;
        if(std::isfinite(member.radius))
            maxRadius=std::max(maxRadius,member.radius);
    }
    centroid=centroid*(1.f/float(n));
    Vec3 forward=desiredCenter-centroid;
    forward.y=0;
    if(forward.lengthXZ()<.001f)forward=Vec3(1.f,0.f,0.f);
    else forward=forward.normalized();
    const Vec3 right(forward.z,0.f,-forward.x);

    const float multiplier=std::isfinite(spacingMultiplier)?
        std::clamp(spacingMultiplier,1.5f,6.f):2.4f;
    const float spacing=std::clamp(maxRadius*multiplier,1.8f,12.f);
    const int columns=std::max(1,int(std::ceil(std::sqrt(double(n)))));
    const int rows=int((n+std::size_t(columns)-1)/std::size_t(columns));
    std::vector<std::size_t> order(n);
    std::iota(order.begin(),order.end(),std::size_t(0));
    auto frontProjection=[&](std::size_t i) {
        const Vec3 offset=members[i].position-centroid;
        return Vec3::dot(offset,forward);
    };
    auto sideProjection=[&](std::size_t i) {
        const Vec3 offset=members[i].position-centroid;
        return Vec3::dot(offset,right);
    };
    // Sort the units front-to-back before slot assignment, then laterally;
    // two input permutations with the same IDs and geometry yield identical
    // results. O(N log N), rather than greedy all-pairs slot matching.
    std::sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b) {
        const float fa=frontProjection(a),fb=frontProjection(b);
        if(std::fabs(fa-fb)>0.0001f)return fa>fb;
        const float sa=sideProjection(a),sb=sideProjection(b);
        if(std::fabs(sa-sb)>0.0001f)return sa<sb;
        return members[a].id<members[b].id;
    });
    for(std::size_t orderIndex=0;orderIndex<n;++orderIndex) {
        const int row=int(orderIndex/std::size_t(columns));
        const int column=int(orderIndex%std::size_t(columns));
        const int rowCount=std::min(columns,int(n)-row*columns);
        const float lateral=(float(column)-float(rowCount-1)*.5f)*spacing;
        const float advance=(float(rows-1)*.5f-float(row))*spacing;
        Vec3 target=desiredCenter+right*lateral+forward*advance;
        target.y=desiredCenter.y;
        const std::size_t memberIndex=order[orderIndex];
        result[memberIndex]={members[memberIndex].id,target};
    }
    return result;
}
