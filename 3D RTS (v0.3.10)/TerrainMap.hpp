#pragma once
#include "Math3D.hpp"
#include "Ray.hpp"
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class TerrainMap {
public:
    enum class Brush { Raise, Lower, Smooth, Flatten, Paint, Block, Unblock };
    struct Cell {
        float height = 0.0f;
        std::uint8_t material = 0;
        bool blocked = false;
    };
    struct Snapshot { std::vector<Cell> cells; };
    TerrainMap(int width=49, int depth=49, float spacing=2.0f,
               float minX=-48.0f,float minZ=-48.0f);

    int width() const { return width_; }
    int depth() const { return depth_; }
    float spacing() const { return spacing_; }
    float minX() const { return minX_; }
    float minZ() const { return minZ_; }
    bool inBounds(int x,int z) const;
    bool insideWorld(float x,float z) const;
    const Cell& cell(int x,int z) const;
    bool setCell(int x,int z,const Cell& v);
    float sampleHeight(float x,float z) const;
    Vec3 clampToMap(const Vec3& p) const;
    bool raycast(const Ray& ray,Vec3& intersection,float maxDistance=350.0f) const;

    void beginStroke();
    bool applyBrush(const Vec3& center,Brush brush,float radius,float strength,
                    std::uint8_t material=0);
    void endStroke();
    bool undo();
    bool redo();
    void clearHistory();
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_=false; }
    std::vector<float> buildMesh() const; // x,y,z,material per vertex; triangles
    bool valid() const { return width_>=2 && depth_>=2; }
private:
    int width_=49,depth_=49;
    float spacing_=2.0f,minX_=-48.0f,minZ_=-48.0f;
    std::vector<Cell> cells_;
    std::deque<Snapshot> undo_,redo_;
    bool stroke_=false,strokeChanged_=false,dirty_=true;
    std::size_t index(int x,int z) const { return static_cast<std::size_t>(z*width_+x); }
    Cell& at(int x,int z) { return cells_[index(x,z)]; }
    static constexpr std::size_t maxHistory_ = 32;
};
