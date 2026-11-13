#include "NavigationGrid.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

NavigationGrid::NavigationGrid(int w, int h, float step, float x, float z)
    : width_(std::max(1,w)), height_(std::max(1,h)),
      step_(std::max(0.1f,step)), minX_(x), minZ_(z),
      blocked_(static_cast<size_t>(width_ * height_),0) {}

bool NavigationGrid::inBounds(int x,int z) const {
    return x >= 0 && z >= 0 && x < width_ && z < height_;
}
bool NavigationGrid::walkable(int x,int z) const {
    return inBounds(x,z) && blocked_[static_cast<size_t>(z*width_+x)] == 0;
}
void NavigationGrid::setBlocked(int x,int z,bool b) {
    if (inBounds(x,z)) blocked_[static_cast<size_t>(z*width_+x)] = b ? 1 : 0;
}
NavigationGrid::Cell NavigationGrid::toCell(const Vec3& p) const {
    return {std::clamp(static_cast<int>(std::floor((p.x-minX_)/step_)),0,width_-1),
            std::clamp(static_cast<int>(std::floor((p.z-minZ_)/step_)),0,height_-1)};
}
Vec3 NavigationGrid::centerOf(Cell c) const {
    return {minX_+(c.x+0.5f)*step_,0,minZ_+(c.z+0.5f)*step_};
}
Vec3 NavigationGrid::clampWorld(const Vec3& p) const {
    return {std::clamp(p.x,minX_+0.5f*step_,minX_+(width_-0.5f)*step_),0,
            std::clamp(p.z,minZ_+0.5f*step_,minZ_+(height_-0.5f)*step_)};
}
void NavigationGrid::blockDisc(const Vec3& p,float radius) {
    const float r2=radius*radius;
    for (int z=0;z<height_;++z)
        for (int x=0;x<width_;++x) {
            Vec3 c=centerOf({x,z});
            float dx=c.x-p.x,dz=c.z-p.z;
            if(dx*dx+dz*dz<=r2) setBlocked(x,z,true);
        }
}
bool NavigationGrid::nearestWalkable(Cell from,Cell& out) const {
    if(walkable(from.x,from.z)){out=from;return true;}
    // Breadth-first search, bounded by the map; guarantees termination.
    std::queue<Cell> q;
    std::vector<uint8_t> seen(blocked_.size(),0);
    q.push(from); seen[static_cast<size_t>(index(from))]=1;
    const int dx[]={0,1,0,-1},dz[]={-1,0,1,0};
    while(!q.empty()) {
        Cell c=q.front();q.pop();
        for(int k=0;k<4;++k){
            Cell n{c.x+dx[k],c.z+dz[k]};
            if(!inBounds(n.x,n.z) || seen[static_cast<size_t>(index(n))]) continue;
            if(walkable(n.x,n.z)){out=n;return true;}
            seen[static_cast<size_t>(index(n))]=1;q.push(n);
        }
    }
    return false;
}
std::vector<Vec3> NavigationGrid::findPath(const Vec3& start,const Vec3& target) const {
    Cell begin=toCell(start),goal=toCell(target);
    if(!nearestWalkable(begin,begin) || !nearestWalkable(goal,goal)) return {};
    const int n=width_*height_,a=index(begin),b=index(goal);
    if(a==b) return {centerOf(goal)};
    struct Node{float f;int id;};
    struct Cmp{bool operator()(const Node& l,const Node& r) const {
        return l.f==r.f ? l.id>r.id : l.f>r.f;
    }};
    std::priority_queue<Node,std::vector<Node>,Cmp> open;
    std::vector<float> dist(static_cast<size_t>(n),std::numeric_limits<float>::infinity());
    std::vector<int> parent(static_cast<size_t>(n),-1);
    std::vector<uint8_t> closed(static_cast<size_t>(n),0);
    auto heuristic=[&](Cell p) {
        int dx=std::abs(p.x-goal.x),dz=std::abs(p.z-goal.z);
        return float(std::max(dx,dz)) + 0.41421356f*float(std::min(dx,dz));
    };
    dist[static_cast<size_t>(a)]=0;open.push({heuristic(begin),a});
    const int dx[]={0,1,0,-1,1,1,-1,-1};
    const int dz[]={-1,0,1,0,-1,1,1,-1};
    while(!open.empty()){
        int id=open.top().id;open.pop();
        if(closed[static_cast<size_t>(id)])continue;
        closed[static_cast<size_t>(id)]=1;
        if(id==b)break;
        Cell c{id%width_,id/width_};
        for(int i=0;i<8;++i){
            Cell next{c.x+dx[i],c.z+dz[i]};
            if(!walkable(next.x,next.z))continue;
            if(i>=4 && (!walkable(c.x+dx[i],c.z) || !walkable(c.x,c.z+dz[i]))) continue;
            int nid=index(next);
            float candidate=dist[static_cast<size_t>(id)]+(i<4 ? 1.0f:1.41421356f);
            if(candidate+0.00001f<dist[static_cast<size_t>(nid)]){
                dist[static_cast<size_t>(nid)]=candidate;
                parent[static_cast<size_t>(nid)]=id;
                open.push({candidate+heuristic(next),nid});
            }
        }
    }
    if(parent[static_cast<size_t>(b)]<0)return {};
    std::vector<Vec3> reversed;
    for(int i=b;i!=a;i=parent[static_cast<size_t>(i)]) {
        if(i<0)return {};
        reversed.push_back(centerOf({i%width_,i/width_}));
    }
    std::reverse(reversed.begin(),reversed.end());
    return reversed;
}
