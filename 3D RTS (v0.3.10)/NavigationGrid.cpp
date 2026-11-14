#include "NavigationGrid.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

NavigationGrid::NavigationGrid(int w, int h, float step, float x, float z)
    : width_(std::max(1,w)), height_(std::max(1,h)),
      step_(std::max(0.1f,step)), minX_(x), minZ_(z),
      blocked_(static_cast<size_t>(width_ * height_),0),
      cost_(static_cast<size_t>(width_ * height_),1.f) {}

bool NavigationGrid::inBounds(int x,int z) const {
    return x >= 0 && z >= 0 && x < width_ && z < height_;
}
bool NavigationGrid::walkable(int x,int z) const {
    return inBounds(x,z) && blocked_[static_cast<size_t>(z*width_+x)] == 0;
}
void NavigationGrid::setBlocked(int x,int z,bool b) {
    if(!inBounds(x,z))return;
    auto& cell=blocked_[static_cast<size_t>(z*width_+x)];
    const std::uint8_t desired=b?1:0;
    if(cell!=desired){cell=desired;++revision_;}
}
void NavigationGrid::setTraversalCost(int x,int z,float cost) {
    if(inBounds(x,z) && std::isfinite(cost) && cost>=1.f && cost<=1000.f){
        auto& previous=cost_[static_cast<size_t>(z*width_+x)];
        if(previous!=cost){previous=cost;++revision_;}
    }
}
float NavigationGrid::traversalCost(int x,int z)const {
    return inBounds(x,z)?cost_[static_cast<size_t>(z*width_+x)]
                         :std::numeric_limits<float>::infinity();
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
    if(!std::isfinite(p.x) || !std::isfinite(p.z) ||
       !std::isfinite(radius) || radius<0.f)return;
    // Dynamic structure/mineral footprints only touch the cells intersecting
    // their local AABB. Avoid O(map width * map height) per placement.
    const double x0=(double(p.x)-radius-double(minX_))/double(step_);
    const double x1=(double(p.x)+radius-double(minX_))/double(step_);
    const double z0=(double(p.z)-radius-double(minZ_))/double(step_);
    const double z1=(double(p.z)+radius-double(minZ_))/double(step_);
    if(x1<0. || z1<0. || x0>=width_ || z0>=height_)return;
    auto bounded=[&](double v,int maxIndex){
        return int(std::clamp(std::floor(v),0.,double(maxIndex)));
    };
    const int minCellX=bounded(x0,width_-1),maxCellX=bounded(x1,width_-1);
    const int minCellZ=bounded(z0,height_-1),maxCellZ=bounded(z1,height_-1);
    const double radiusSquared=double(radius)*radius;
    for(int z=minCellZ;z<=maxCellZ;++z)
        for(int x=minCellX;x<=maxCellX;++x){
            const Vec3 c=centerOf({x,z});
            const double dx=double(c.x)-p.x,dz=double(c.z)-p.z;
            if(dx*dx+dz*dz<=radiusSquared)setBlocked(x,z,true);
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
            // Edge weight is symmetric and >= geometric distance; the octile
            // heuristic stays admissible for weighted terrain.
            float average=(cost_[static_cast<size_t>(id)]+cost_[static_cast<size_t>(nid)])*.5f;
            float candidate=dist[static_cast<size_t>(id)]+
                            (i<4 ? 1.0f:1.41421356f)*average;
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

bool NavigationGrid::routeClear(const Vec3& current,
                                const std::vector<Vec3>& points,
                                std::size_t next)const{
    if(next>=points.size())return true;
    Cell before=toCell(current);
    for(std::size_t i=next;i<points.size();++i){
        const Cell after=toCell(points[i]);
        if(!walkable(after.x,after.z))return false;
        const int dx=after.x-before.x,dz=after.z-before.z;
        // A* paths are contiguous 8-way steps. If steering pushed a unit
        // farther away, replan rather than allow a direct leap through walls.
        if(std::abs(dx)>1 || std::abs(dz)>1)return false;
        if(dx!=0 && dz!=0 &&
           (!walkable(before.x+dx,before.z) ||
            !walkable(before.x,before.z+dz)))return false;
        before=after;
    }
    return true;
}
