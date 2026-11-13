#pragma once
#include "TerrainMap.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

class Unit;

// Player/ally-team vision, held outside the renderer so visibility is usable
// by targeting, replay, triggers, minimap and (later) networking.
// Cells: Unexplored=0, Explored=1, Visible=2. Explored state persists until reset.
class RTSVision {
public:
    enum class State : std::uint8_t { Unexplored=0, Explored=1, Visible=2 };
    void reset(const TerrainMap& terrain);
    void update(const TerrainMap& terrain,const std::vector<Unit>& units,int owner=0);
    State cell(int x,int z) const;
    State atWorld(const Vec3& point) const;
    bool visible(const Vec3& point) const { return atWorld(point)==State::Visible; }
    bool explored(const Vec3& point) const { return atWorld(point)!=State::Unexplored; }
    int width() const { return width_; }
    int depth() const { return depth_; }
    std::size_t revision() const { return revision_; }
    // R8 normalized texture: 0 unexplored, 96 explored, 255 visible.
    // Recomputed only when cells changed; 1 texel per terrain cell.
    const std::vector<std::uint8_t>& textureBytes() const { return pixels_; }
private:
    int width_=0,depth_=0;
    float minX_=0,minZ_=0,spacing_=1;
    std::vector<State> cells_;
    std::vector<std::uint8_t> pixels_;
    std::size_t revision_=0;
    std::size_t index(int x,int z)const {
        return static_cast<std::size_t>(z*width_+x);
    }
    bool lineClear(const TerrainMap& terrain,const Vec3& source,
                   int dstX,int dstZ) const;
};
