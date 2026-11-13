#include "RTSVision.hpp"
#include "Unit.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

void RTSVision::reset(const TerrainMap& map) {
    width_=map.width();depth_=map.depth();
    minX_=map.minX();minZ_=map.minZ();spacing_=map.spacing();
    cells_.assign(static_cast<std::size_t>(width_)*depth_,State::Unexplored);
    pixels_.assign(cells_.size(),0);
    ++revision_;
}
RTSVision::State RTSVision::cell(int x,int z)const {
    if(x<0||z<0||x>=width_||z>=depth_)return State::Unexplored;
    return cells_[index(x,z)];
}
RTSVision::State RTSVision::atWorld(const Vec3& point) const {
    if(width_<1||depth_<1||!std::isfinite(point.x)||!std::isfinite(point.z))
        return State::Unexplored;
    const int x=static_cast<int>(std::floor((point.x-minX_)/spacing_+.5f));
    const int z=static_cast<int>(std::floor((point.z-minZ_)/spacing_+.5f));
    return cell(x,z);
}
bool RTSVision::lineClear(const TerrainMap& map,const Vec3& eye,
                           int tx,int tz) const {
    const float destX=minX_+tx*spacing_;
    const float destZ=minZ_+tz*spacing_;
    const float endHeight=map.cell(tx,tz).height+1.1f;
    const float dx=destX-eye.x,dz=destZ-eye.z;
    const float distance=std::sqrt(dx*dx+dz*dz);
    if(distance<spacing_*1.6f)return true;
    // Half-cell sampling catches narrow ridges without a full raycast.
    const int steps=static_cast<int>(std::ceil(distance/(spacing_*.5f)));
    for(int k=1;k<steps;++k) {
        const float t=float(k)/float(steps);
        const float x=eye.x+dx*t,z=eye.z+dz*t;
        const float rayHeight=eye.y+(endHeight-eye.y)*t;
        if(map.sampleHeight(x,z)>rayHeight+.30f)return false;
    }
    return true;
}
void RTSVision::update(const TerrainMap& terrain,const std::vector<Unit>& units,int owner) {
    if(width_!=terrain.width() || depth_!=terrain.depth() ||
       minX_!=terrain.minX() || minZ_!=terrain.minZ() ||
       spacing_!=terrain.spacing() || cells_.empty())
        reset(terrain);
    // Downgrade only the transient visible state, preserving exploration.
    std::vector<State> next=cells_;
    for(auto& state:next)if(state==State::Visible)state=State::Explored;
    for(const auto& unit:units) {
        if(unit.ownerId!=owner || !unit.isAlive())continue;
        const float radius=std::clamp(unit.data.sight_range,0.f,128.f);
        const Vec3 eye(unit.position.x,
                       terrain.sampleHeight(unit.position.x,unit.position.z)+
                       std::max(1.1f,unit.height*.85f),
                       unit.position.z);
        const int minCellX=std::max(0,int(std::floor((eye.x-radius-minX_)/spacing_)));
        const int maxCellX=std::min(width_-1,int(std::ceil((eye.x+radius-minX_)/spacing_)));
        const int minCellZ=std::max(0,int(std::floor((eye.z-radius-minZ_)/spacing_)));
        const int maxCellZ=std::min(depth_-1,int(std::ceil((eye.z+radius-minZ_)/spacing_)));
        for(int z=minCellZ;z<=maxCellZ;++z)for(int x=minCellX;x<=maxCellX;++x) {
            const float px=minX_+x*spacing_,pz=minZ_+z*spacing_;
            const float dx=px-eye.x,dz=pz-eye.z;
            if(dx*dx+dz*dz>radius*radius)continue;
            const auto at=index(x,z);
            if(next[at]==State::Visible)continue; // allied units share vision
            if(lineClear(terrain,eye,x,z))next[at]=State::Visible;
        }
        // Always reveal the cell occupied by the unit, even if the unit was
        // placed outside an unusually small sight radius.
        const int ux=int(std::round((eye.x-minX_)/spacing_));
        const int uz=int(std::round((eye.z-minZ_)/spacing_));
        if(ux>=0&&ux<width_&&uz>=0&&uz<depth_)next[index(ux,uz)]=State::Visible;
    }
    if(next!=cells_) {
        cells_.swap(next);
        for(std::size_t i=0;i<cells_.size();++i) {
            pixels_[i]=cells_[i]==State::Visible?255:
                       cells_[i]==State::Explored?96:0;
        }
        ++revision_;
    }
}
