#pragma once
// Bounded, GPU-independent static asset reader. RTSMESH1 remains byte-for-byte
// compatible; RTSMESH2 adds multiple indexed material surfaces.
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

struct RTSStaticSurface {
    std::uint32_t firstIndex=0,indexCount=0;
    std::uint32_t width=0,height=0;
    std::array<float,4> baseColor{{1,1,1,1}};
    std::vector<unsigned char> albedo;
};
struct RTSStaticMesh {
    std::vector<float> vertices;              // XYZ, normal XYZ, UV0 XY
    std::vector<std::uint32_t> indices;        // triangles
    std::vector<unsigned char> albedo;         // legacy RTSMESH1 only
    std::uint32_t width=0,height=0;            // legacy RTSMESH1 only
    std::array<float,4> baseColor{{1,1,1,1}};  // legacy RTSMESH1 only
    std::vector<RTSStaticSurface> surfaces;    // RTSMESH2 only
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
    for(const auto& part:relative)
        if(part=="..")throw std::invalid_argument("Mesh asset traversal");
    return result;
}
inline RTSStaticMesh rtsReadStaticMesh(const std::filesystem::path& path){
    namespace fs=std::filesystem;
    constexpr std::uint64_t maxFileSize=130ull*1024*1024;
    std::error_code error;
    const auto size=fs::file_size(path,error);
    if(error || size<24 || size>maxFileSize)
        throw std::invalid_argument("Missing or oversized RTS mesh asset");
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::invalid_argument("Unable to read RTS mesh asset");
    const auto read=[&](void* dst,std::size_t count){
        if(count && !in.read(reinterpret_cast<char*>(dst),
                             static_cast<std::streamsize>(count)))
            throw std::invalid_argument("Truncated RTS mesh asset");
    };
    char magic[8]{};
    read(magic,8);
    const std::string version(magic,8);
    if(version!="RTSMESH1" && version!="RTSMESH2")
        throw std::invalid_argument("Unrecognized RTS mesh version");
    std::uint32_t vertices=0,indices=0,third=0,fourth=0;
    read(&vertices,4);read(&indices,4);read(&third,4);read(&fourth,4);
    if(vertices<3 || vertices>250000 || indices<3 || indices>750000 ||
       indices%3!=0)
        throw std::invalid_argument("Invalid RTS mesh geometry counts");
    RTSStaticMesh mesh;
    if(version=="RTSMESH1"){
        if(size<40 || (third==0)!=(fourth==0) ||
           third>2048 || fourth>2048)
            throw std::invalid_argument("Invalid RTSMESH1 material dimensions");
        for(float& f:mesh.baseColor)read(&f,4);
        for(float f:mesh.baseColor)
            if(!std::isfinite(f) || f<0 || f>1)
                throw std::invalid_argument("Invalid RTSMESH1 material factor");
        const std::uint64_t expected=40ull+
            static_cast<std::uint64_t>(vertices)*32+
            static_cast<std::uint64_t>(indices)*4+
            static_cast<std::uint64_t>(third)*fourth*4;
        if(size!=expected)
            throw std::invalid_argument("RTSMESH1 length mismatch or trailing bytes");
        mesh.width=third;mesh.height=fourth;
    }else if(fourth!=0 || third==0 || third>32)
        throw std::invalid_argument("RTSMESH2 requires 1..32 surfaces and zero reserved field");
    const std::uint64_t geometryBytes=
        static_cast<std::uint64_t>(vertices)*32+
        static_cast<std::uint64_t>(indices)*4;
    const std::uint64_t geometryEnd=(version=="RTSMESH1"?40ull:24ull)+geometryBytes;
    if(geometryEnd>size)
        throw std::invalid_argument("Truncated RTS mesh geometry");
    mesh.vertices.resize(static_cast<std::size_t>(vertices)*8);
    mesh.indices.resize(indices);
    read(mesh.vertices.data(),mesh.vertices.size()*sizeof(float));
    read(mesh.indices.data(),mesh.indices.size()*sizeof(std::uint32_t));
    for(float v:mesh.vertices)
        if(!std::isfinite(v) || std::fabs(v)>1e7f)
            throw std::invalid_argument("RTS mesh contains invalid vertex");
    for(const auto index:mesh.indices)
        if(index>=vertices)
            throw std::invalid_argument("RTS mesh index outside vertex buffer");
    if(version=="RTSMESH1"){
        mesh.albedo.resize(static_cast<std::size_t>(third)*fourth*4);
        read(mesh.albedo.data(),mesh.albedo.size());
        return mesh;
    }
    std::uint64_t consumed=geometryEnd;
    std::uint32_t coveredIndices=0;
    mesh.surfaces.reserve(third);
    for(std::uint32_t s=0;s<third;++s){
        if(size-consumed<32)
            throw std::invalid_argument("Truncated RTSMESH2 surface header");
        RTSStaticSurface part;
        read(&part.firstIndex,4);read(&part.indexCount,4);
        read(&part.width,4);read(&part.height,4);
        for(float& color:part.baseColor)read(&color,4);
        consumed+=32;
        if(part.firstIndex!=coveredIndices || part.indexCount<3 ||
           part.indexCount%3 || part.indexCount>indices-coveredIndices ||
           (part.width==0)!=(part.height==0) ||
           part.width>2048 || part.height>2048)
            throw std::invalid_argument("Invalid or overlapping RTSMESH2 draw ranges");
        for(float f:part.baseColor)
            if(!std::isfinite(f) || f<0 || f>1)
                throw std::invalid_argument("Invalid RTSMESH2 material factor");
        const auto texBytes=static_cast<std::uint64_t>(part.width)*part.height*4;
        if(texBytes>size-consumed)
            throw std::invalid_argument("Truncated RTSMESH2 material image");
        part.albedo.resize(static_cast<std::size_t>(texBytes));
        read(part.albedo.data(),part.albedo.size());
        consumed+=texBytes;
        coveredIndices+=part.indexCount;
        mesh.surfaces.emplace_back(std::move(part));
    }
    if(coveredIndices!=indices || consumed!=size)
        throw std::invalid_argument("RTSMESH2 triangle coverage or file length mismatch");
    return mesh;
}
