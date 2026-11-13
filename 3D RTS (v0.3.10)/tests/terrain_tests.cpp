#include "TerrainMap.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    TerrainMap terrain(9,9,2,-8,-8);
    assert(terrain.width()==9 && terrain.depth()==9);
    assert(terrain.buildMesh().size()==8u*8u*6u*5u);
    terrain.beginStroke();
    assert(terrain.applyBrush({0,0,0},TerrainMap::Brush::Raise,4,2));
    terrain.endStroke();
    assert(terrain.sampleHeight(0,0)>1.9f);
    assert(terrain.sampleHeight(7,7)==0);
    const auto raised=terrain.sampleHeight(0,0);
    assert(terrain.undo() && terrain.sampleHeight(0,0)==0);
    assert(terrain.redo() && terrain.sampleHeight(0,0)==raised);

    terrain.beginStroke();
    assert(terrain.applyBrush({0,raised,0},TerrainMap::Brush::Paint,4,1,3));
    terrain.endStroke();
    assert(terrain.cell(4,4).material==3);
    assert(terrain.undo());
    assert(terrain.cell(4,4).material==0);
    assert(terrain.redo());
    assert(terrain.cell(4,4).material==3);

    terrain.beginStroke();
    assert(terrain.applyBrush({0,raised,0},TerrainMap::Brush::Block,4,1));
    terrain.endStroke();
    assert(terrain.cell(4,4).blocked);
    terrain.undo();
    assert(!terrain.cell(4,4).blocked);

    Ray down={{0,20,0},{0,-1,0}};
    Vec3 hit;
    assert(terrain.raycast(down,hit));
    assert(std::abs(hit.y-terrain.sampleHeight(0,0))<0.02f);
    assert(!terrain.raycast({{100,20,100},{0,-1,0}},hit));
    std::cout<<"terrain: brushes, undo/redo, mesh and raycast passed\n";
}
