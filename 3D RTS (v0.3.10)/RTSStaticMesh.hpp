#pragma once
// Standalone bounded CPU static mesh reader: no OpenGL or simulation deps.
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

struct RTSStaticMesh {
    std::vector<float> vertices;       // XYZ, normals XYZ, UV XY
    std::vector<std::uint32_t> indices;
    std::vector<unsigned char> albedo;
    std::uint32_t width=0,height=0;
    std::array<float,4> baseColor{{1,1,1,1}};
};
inline bool rtsSafeMeshReference(const std::string& name){
    if(name.size()<16 || name.size()>180 || name.compare(0,7,"assets/")!=0 ||
       name.substr(name.size()-8)!=".rtsmesh" ||
       name.find("//")!=std::string::npos)return false;
    for(char c:name)
        if(!((c>='A'&&c<='Z') || (c>='a'&&c<='z') ||
             (c>='0'&&c<='9') || c=='/' || c=='_' || c=='-' || c=='.'))
            return false;
    std::size_t from=0;
    for(;;){
        const auto end=name.find('/',from);
        const auto part=name.substr(from,end-from);
        if(part.empty() || part=="." || part=="..")return false;
        if(end==std::string::npos)break;
        from=end+1;
    }
    return true;
}
inline std::filesystem::path rtsResolveMeshReference(
    const std::filesystem::path& project,const std::string& reference){
    if(!rtsSafeMeshReference(reference))
        throw std::invalid_argument("Mesh reference must be assets/*.rtsmesh");
    namespace fs=std::filesystem;
    const auto root=fs::weakly_canonical(fs::absolute(project).parent_path());
    const auto result=fs::weakly_canonical(root/reference);
    std::error_code error;
    const auto relative=fs::relative(result,root,error);
    if(error || relative.empty() || relative.is_absolute())
        throw std::invalid_argument("Mesh asset path escapes project");
    for(const auto& component:relative)
        if(component=="..")throw std::invalid_argument("Mesh asset traversal");
    return result;
}
inline RTSStaticMesh rtsReadStaticMesh(const std::filesystem::path& path){
    namespace fs=std::filesystem;
    std::error_code error;
    const auto size=fs::file_size(path,error);
    if(error || size<40 || size>130ull*1024*1024)
        throw std::invalid_argument("Missing or oversized RTSMESH1 asset");
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::invalid_argument("Unable to read mesh asset");
    const auto read=[&](void* dst,std::size_t count){
        if(count && !in.read(reinterpret_cast<char*>(dst),
                             static_cast<std::streamsize>(count)))
            throw std::invalid_argument("Truncated RTSMESH1 asset");
    };
    char magic[8]{};
    read(magic,8);
    if(std::string(magic,8)!="RTSMESH1")
        throw std::invalid_argument("Unrecognized mesh version");
    std::uint32_t vertices=0,indices=0,w=0,h=0;
    read(&vertices,4);read(&indices,4);read(&w,4);read(&h,4);
    RTSStaticMesh mesh;
    for(auto& f:mesh.baseColor)read(&f,4);
    if(vertices<3 || vertices>250000 || indices<3 || indices>750000 ||
       indices%3 || (w==0)!=(h==0) || w>2048 || h>2048)
        throw std::invalid_argument("Invalid mesh or image dimensions");
    for(float f:mesh.baseColor)
        if(!std::isfinite(f) || f<0 || f>1)
            throw std::invalid_argument("Invalid material factor");
    const std::uint64_t required=40ull+
        static_cast<std::uint64_t>(vertices)*8*4+
        static_cast<std::uint64_t>(indices)*4+
        static_cast<std::uint64_t>(w)*h*4;
    if(size!=required)
        throw std::invalid_argument("Mesh length mismatch or trailing bytes");
    mesh.vertices.resize(static_cast<std::size_t>(vertices)*8);
    mesh.indices.resize(indices);
    mesh.albedo.resize(static_cast<std::size_t>(w)*h*4);
    mesh.width=w;mesh.height=h;
    read(mesh.vertices.data(),mesh.vertices.size()*4);
    read(mesh.indices.data(),mesh.indices.size()*4);
    read(mesh.albedo.data(),mesh.albedo.size());
    for(float v:mesh.vertices)
        if(!std::isfinite(v) || std::fabs(v)>1e7f)
            throw std::invalid_argument("Mesh contains invalid vertex");
    for(auto index:mesh.indices)
        if(index>=vertices)
            throw std::invalid_argument("Mesh index outside vertex buffer");
    return mesh;
}
