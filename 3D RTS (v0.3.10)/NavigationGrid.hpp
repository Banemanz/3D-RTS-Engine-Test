#pragma once
#include "Math3D.hpp"
#include <cstdint>
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
    void blockDisc(const Vec3& center, float radius);
    Cell toCell(const Vec3& p) const;
    Vec3 centerOf(Cell c) const;
    Vec3 clampWorld(const Vec3& p) const;
    std::vector<Vec3> findPath(const Vec3& start, const Vec3& target) const;
    int width() const { return width_; }
    int height() const { return height_; }
private:
    int width_, height_;
    float step_, minX_, minZ_;
    std::vector<uint8_t> blocked_;
    int index(Cell c) const { return c.z * width_ + c.x; }
    bool nearestWalkable(Cell origin, Cell& result) const;
};
