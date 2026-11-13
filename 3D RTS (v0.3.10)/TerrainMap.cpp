#include "TerrainMap.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

TerrainMap::TerrainMap(int width,int depth,float spacing,float minX,float minZ)
    : width_(std::clamp(width,2,513)), depth_(std::clamp(depth,2,513)),
      spacing_(std::clamp(spacing,0.25f,64.0f)),minX_(minX),minZ_(minZ),
      cells_(static_cast<std::size_t>(width_*depth_)) {}

bool TerrainMap::inBounds(int x,int z)const {
    return x>=0 && z>=0 && x<width_ && z<depth_;
}
bool TerrainMap::insideWorld(float x,float z)const {
    return x>=minX_ && z>=minZ_ &&
           x<=minX_+(width_-1)*spacing_ &&
           z<=minZ_+(depth_-1)*spacing_;
}
const TerrainMap::Cell& TerrainMap::cell(int x,int z)const {
    return cells_[index(std::clamp(x,0,width_-1),std::clamp(z,0,depth_-1))];
}
bool TerrainMap::setCell(int x,int z,const Cell& v) {
    if(!inBounds(x,z) || !std::isfinite(v.height) || std::abs(v.height)>512 ||
       v.material>7) return false;
    at(x,z)=v;dirty_=true;return true;
}
Vec3 TerrainMap::clampToMap(const Vec3& p)const {
    const float x=std::clamp(p.x,minX_,minX_+(width_-1)*spacing_);
    const float z=std::clamp(p.z,minZ_,minZ_+(depth_-1)*spacing_);
    return Vec3(x,sampleHeight(x,z),z);
}
float TerrainMap::sampleHeight(float x,float z)const {
    float fx=std::clamp((x-minX_)/spacing_,0.0f,float(width_-1));
    float fz=std::clamp((z-minZ_)/spacing_,0.0f,float(depth_-1));
    int xi=static_cast<int>(fx),zi=static_cast<int>(fz);
    int x2=std::min(xi+1,width_-1),z2=std::min(zi+1,depth_-1);
    float a=fx-xi,b=fz-zi;
    const float h0=lerp(cell(xi,zi).height,cell(x2,zi).height,a);
    const float h1=lerp(cell(xi,z2).height,cell(x2,z2).height,a);
    return lerp(h0,h1,b);
}
bool TerrainMap::raycast(const Ray& ray,Vec3& result,float maxDistance)const {
    // Stepping finds the first above->below surface crossing; binary search
    // sharpens it to about a millimeter. Unlike y=0 picking this follows sculpted hills.
    const float step=std::max(0.25f,spacing_*0.4f);
    bool previousValid=false;
    float prevT=0,prevDiff=0;
    for(float t=0;t<=maxDistance;t+=step) {
        Vec3 p=ray.origin+ray.direction*t;
        if(!insideWorld(p.x,p.z)){previousValid=false;continue;}
        float diff=p.y-sampleHeight(p.x,p.z);
        if(previousValid && prevDiff>=0 && diff<=0) {
            float lo=prevT,hi=t;
            for(int i=0;i<15;++i) {
                float mid=(lo+hi)*0.5f;
                Vec3 q=ray.origin+ray.direction*mid;
                if(q.y-sampleHeight(q.x,q.z)>0)lo=mid;
                else hi=mid;
            }
            result=ray.origin+ray.direction*((lo+hi)*0.5f);
            result.y=sampleHeight(result.x,result.z);
            return true;
        }
        previousValid=true;prevT=t;prevDiff=diff;
    }
    return false;
}
void TerrainMap::beginStroke() {
    if(stroke_)return;
    stroke_=true;strokeChanged_=false;
    undo_.push_back({cells_});
    while(undo_.size()>maxHistory_)undo_.pop_front();
}
bool TerrainMap::applyBrush(const Vec3& center,Brush brush,float radius,
                            float strength,std::uint8_t material) {
    if(!stroke_ || !insideWorld(center.x,center.z) || !std::isfinite(strength))return false;
    radius=std::clamp(radius,0.25f,32.0f);
    strength=std::clamp(strength,0.0f,32.0f);
    if(material>7)return false;
    std::vector<Cell> before=cells_; // smooth uses a stable source snapshot
    bool changed=false;
    int minCx=std::max(0,int(std::floor((center.x-radius-minX_)/spacing_)));
    int maxCx=std::min(width_-1,int(std::ceil((center.x+radius-minX_)/spacing_)));
    int minCz=std::max(0,int(std::floor((center.z-radius-minZ_)/spacing_)));
    int maxCz=std::min(depth_-1,int(std::ceil((center.z+radius-minZ_)/spacing_)));
    for(int z=minCz;z<=maxCz;++z)
        for(int x=minCx;x<=maxCx;++x) {
            float px=minX_+x*spacing_,pz=minZ_+z*spacing_;
            float distance=std::hypot(px-center.x,pz-center.z);
            if(distance>radius)continue;
            float influence=(1.0f-distance/radius);
            influence=influence*influence*(3.0f-2.0f*influence);
            Cell& v=at(x,z);
            float prior=v.height;
            switch(brush) {
                case Brush::Raise: v.height=std::min(60.f,v.height+strength*influence);break;
                case Brush::Lower: v.height=std::max(-60.f,v.height-strength*influence);break;
                case Brush::Flatten: v.height=lerp(v.height,center.y,std::min(1.f,strength*influence));break;
                case Brush::Smooth: {
                    float sum=0;int n=0;
                    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)
                        if(inBounds(x+dx,z+dz)){sum+=before[index(x+dx,z+dz)].height;++n;}
                    v.height=lerp(prior,sum/n,std::min(1.f,strength*influence));
                    break;
                }
                case Brush::Paint: if(influence>0.08f && v.material!=material) {
                    v.material=material;changed=true;
                }break;
                case Brush::Block: if(influence>0.08f && !v.blocked){
                    v.blocked=true;changed=true;
                }break;
                case Brush::Unblock: if(influence>0.08f && v.blocked){
                    v.blocked=false;changed=true;
                }break;
            }
            if(v.height!=prior)changed=true;
        }
    if(changed) {
        dirty_=true;strokeChanged_=true;
        redo_.clear();
    }
    return changed;
}
void TerrainMap::endStroke(){
    if(!stroke_)return;
    stroke_=false;
    if(!strokeChanged_ && !undo_.empty())undo_.pop_back();
}
bool TerrainMap::undo(){
    endStroke();
    if(undo_.empty())return false;
    redo_.push_back({cells_});
    cells_=std::move(undo_.back().cells);undo_.pop_back();
    dirty_=true;return true;
}
bool TerrainMap::redo(){
    endStroke();
    if(redo_.empty())return false;
    undo_.push_back({cells_});
    if(undo_.size()>maxHistory_)undo_.pop_front();
    cells_=std::move(redo_.back().cells);redo_.pop_back();
    dirty_=true;return true;
}
void TerrainMap::clearHistory(){undo_.clear();redo_.clear();stroke_=false;strokeChanged_=false;}
std::vector<float> TerrainMap::buildMesh() const {
    std::vector<float> result;
    result.reserve(static_cast<std::size_t>((width_-1)*(depth_-1)*6*5));
    auto push=[&](int x,int z,unsigned char material) {
        result.push_back(minX_+x*spacing_);
        result.push_back(cell(x,z).height);
        result.push_back(minZ_+z*spacing_);
        result.push_back(float(material));
        result.push_back(cell(x,z).blocked?1.0f:0.0f);
    };
    for(int z=0;z<depth_-1;++z)
      for(int x=0;x<width_-1;++x) {
        unsigned char mat=cell(x,z).material;
        push(x,z,mat);push(x+1,z,mat);push(x+1,z+1,mat);
        push(x,z,mat);push(x+1,z+1,mat);push(x,z+1,mat);
    }
    return result;
}
