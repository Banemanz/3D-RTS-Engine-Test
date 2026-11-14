#pragma once
#include "Math3D.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class Unit;

// Deterministic XZ broadphase for local steering, aggro and stable ID lookup.
// Buckets are updated incrementally when units move. Queries always return
// indices in source-vector order to preserve tie-breaking / repeatability.
class UnitSpatialIndex {
public:
    explicit UnitSpatialIndex(float cellSize=8.f);
    void rebuild(const std::vector<Unit>& units);
    void moved(std::size_t index,const Vec3& oldPosition,const Vec3& newPosition);
    std::vector<std::size_t> nearby(const Vec3& position,float radius) const;
    Unit* byId(std::vector<Unit>& units,int id) const;
    const Unit* byId(const std::vector<Unit>& units,int id) const;
    float maxStructureRadius()const { return largestStructure_; }
    std::size_t activeCount()const { return living_; }
private:
    static std::uint64_t key(int x,int z);
    std::pair<int,int> coords(const Vec3& pos) const;
    float cellSize_=8.f;
    float largestStructure_=0.f;
    std::size_t living_=0;
    std::unordered_map<std::uint64_t,std::vector<std::size_t>> buckets_;
    std::unordered_map<int,std::size_t> ids_;
};
