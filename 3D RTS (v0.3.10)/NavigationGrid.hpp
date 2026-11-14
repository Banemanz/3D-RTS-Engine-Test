#pragma once
#include "Math3D.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>

// Flat XZ navigation grid. An explicit map is preferable to pretending that
// local steering can find routes around structures.
class NavigationGrid {
public:
    struct Cell { int x = 0; int z = 0; };
    NavigationGrid(int width = 49, int height = 49, float cellSize = 2.0f,
                   float minX = -49.0f, float minZ = -49.0f);
    bool inBounds(int x, int z) const;
    bool walkable(int x, int z) const;
    void setBlocked(int x, int z, bool blocked);
    void setTraversalCost(int x,int z,float cost);
    float traversalCost(int x,int z) const;
    void blockDisc(const Vec3& center, float radius);
    Cell toCell(const Vec3& p) const;
    Vec3 centerOf(Cell c) const;
    Vec3 clampWorld(const Vec3& p) const;
    std::vector<Vec3> findPath(const Vec3& start, const Vec3& target) const;
    // Validate the remaining A* corridor against newly painted/placed blockers,
    // including diagonal corner-cutting. No pathfinding unless it became unsafe.
    bool routeClear(const Vec3& current,const std::vector<Vec3>& points,
                    std::size_t next=0)const;
    std::uint64_t revision()const{return revision_;}
    int width() const { return width_; }
    int height() const { return height_; }
private:
    int width_, height_;
    float step_, minX_, minZ_;
    std::vector<uint8_t> blocked_;
    std::vector<float> cost_;
    std::uint64_t revision_=0;
    int index(Cell c) const { return c.z * width_ + c.x; }
    bool nearestWalkable(Cell origin, Cell& result) const;
};
