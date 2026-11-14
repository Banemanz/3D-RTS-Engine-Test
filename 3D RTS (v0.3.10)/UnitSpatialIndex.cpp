#include "UnitSpatialIndex.hpp"
#include "Unit.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

UnitSpatialIndex::UnitSpatialIndex(float cellSize):
    cellSize_(std::isfinite(cellSize)?std::clamp(cellSize,1.f,128.f):8.f) {}
std::uint64_t UnitSpatialIndex::key(int x,int z){
    return (std::uint64_t(std::uint32_t(x))<<32)|std::uint32_t(z);
}
std::pair<int,int> UnitSpatialIndex::coords(const Vec3& pos) const {
    // Simulation map coordinates are bounded by validated project extents.
    return {int(std::floor(pos.x/cellSize_)),int(std::floor(pos.z/cellSize_))};
}
void UnitSpatialIndex::rebuild(const std::vector<Unit>& units) {
    buckets_.clear();ids_.clear();
    buckets_.reserve(units.size()/3+8);
    ids_.reserve(units.size());
    largestStructure_=0.f;living_=0;
    for(std::size_t i=0;i<units.size();++i) {
        const auto& u=units[i];
        ids_[u.id]=i;
        if(!u.isAlive())continue;
        ++living_;
        if(u.isStructure)largestStructure_=std::max(largestStructure_,u.radius);
        const auto cell=coords(u.position);
        buckets_[key(cell.first,cell.second)].push_back(i);
    }
}
void UnitSpatialIndex::moved(std::size_t index,const Vec3& oldPosition,
                             const Vec3& newPosition) {
    const auto before=coords(oldPosition),after=coords(newPosition);
    if(before==after)return;
    const auto oldKey=key(before.first,before.second);
    auto it=buckets_.find(oldKey);
    if(it!=buckets_.end()){
        auto& group=it->second;
        auto item=std::find(group.begin(),group.end(),index);
        if(item!=group.end())group.erase(item);
        if(group.empty())buckets_.erase(it);
    }
    buckets_[key(after.first,after.second)].push_back(index);
}
std::vector<std::size_t> UnitSpatialIndex::nearby(const Vec3& pos,float radius) const {
    std::vector<std::size_t> result;
    if(!std::isfinite(radius) || radius<0.f)return result;
    // Unusually enormous search ranges should visit each actual bucket,
    // not iterate millions of empty coordinate pairs.
    const float cells=radius/cellSize_;
    if(cells>96.f){
        for(const auto& pair:buckets_)
            result.insert(result.end(),pair.second.begin(),pair.second.end());
    }else{
        const int minX=int(std::floor((pos.x-radius)/cellSize_));
        const int maxX=int(std::floor((pos.x+radius)/cellSize_));
        const int minZ=int(std::floor((pos.z-radius)/cellSize_));
        const int maxZ=int(std::floor((pos.z+radius)/cellSize_));
        for(int z=minZ;z<=maxZ;++z)for(int x=minX;x<=maxX;++x){
            auto found=buckets_.find(key(x,z));
            if(found!=buckets_.end())
                result.insert(result.end(),found->second.begin(),found->second.end());
        }
    }
    std::sort(result.begin(),result.end());
    return result;
}
Unit* UnitSpatialIndex::byId(std::vector<Unit>& units,int id) const {
    const auto found=ids_.find(id);
    if(found==ids_.end()||found->second>=units.size() ||
       units[found->second].id!=id)return nullptr;
    return &units[found->second];
}
const Unit* UnitSpatialIndex::byId(const std::vector<Unit>& units,int id) const {
    const auto found=ids_.find(id);
    if(found==ids_.end()||found->second>=units.size() ||
       units[found->second].id!=id)return nullptr;
    return &units[found->second];
}
