#include "RTSStaticMesh.hpp"
#include <cassert>
#if defined(NDEBUG)
#error "Static mesh tests require assertions in optimized builds"
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
int main(int argc,char** argv){
    if(argc!=4)throw std::invalid_argument("Expected project path, v1 and v2 meshes");
    const std::filesystem::path project=argv[1],source=argv[2],dualFile=argv[3];
    assert(rtsSafeMeshReference("assets/beacon.rtsmesh"));
    assert(!rtsSafeMeshReference("../assets/beacon.rtsmesh"));
    assert(!rtsSafeMeshReference("assets/../secrets.rtsmesh"));
    assert(!rtsSafeMeshReference("assets\\beacon.rtsmesh"));
    assert(rtsResolveMeshReference(project,"assets/beacon.rtsmesh")==
           std::filesystem::weakly_canonical(source));
    const auto mesh=rtsReadStaticMesh(source);
    assert(mesh.vertices.size()==32u*8u && mesh.indices.size()==48u);
    assert(mesh.width==2 && mesh.height==2 && mesh.albedo.size()==16);
    for(auto i:mesh.indices)assert(i<mesh.vertices.size()/8);
    std::ifstream in(source,std::ios::binary);
    const std::vector<char> original((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
    assert(original.size()>50);
    const auto temp=std::filesystem::current_path()/"invalid_mesh_test.rtsmesh";
    const auto mustReject=[&](const std::vector<char>& bytes){
        {std::ofstream out(temp,std::ios::binary|std::ios::trunc);
         out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));}
        bool rejected=false;
        try{(void)rtsReadStaticMesh(temp);}
        catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);
    };
    auto bad=original;bad[0]='X';mustReject(bad);
    bad=original;bad.pop_back();mustReject(bad);
    bad=original;bad.push_back('!');mustReject(bad);
    bad=original;for(int i=8;i<12;++i)bad[static_cast<std::size_t>(i)]=char(255);
    mustReject(bad);

    // New multi-material format retains exactly ordered independent draws.
    const auto dual=rtsReadStaticMesh(dualFile);
    assert(dual.vertices.size()==64u*8u && dual.indices.size()==96u);
    assert(dual.surfaces.size()==2 && dual.albedo.empty());
    assert(dual.surfaces[0].firstIndex==0 && dual.surfaces[0].indexCount==48);
    assert(dual.surfaces[0].width==2 && dual.surfaces[0].height==2);
    assert(dual.surfaces[0].albedo.size()==16);
    assert(dual.surfaces[1].firstIndex==48 && dual.surfaces[1].indexCount==48);
    assert(dual.surfaces[1].width==0 && dual.surfaces[1].albedo.empty());
    assert(dual.surfaces[1].baseColor[0]>.9f &&
           dual.surfaces[1].baseColor[2]<.2f);
    for(auto idx:dual.indices)assert(idx<64);
    std::ifstream dualStream(dualFile,std::ios::binary);
    const std::vector<char> originalDual((std::istreambuf_iterator<char>(dualStream)),
                                         std::istreambuf_iterator<char>());
    assert(originalDual.size()==2536);
    bad=originalDual;bad[20]=1;mustReject(bad);  // Reserved byte
    bad=originalDual;bad[16]=33;mustReject(bad); // Too many surfaces
    constexpr std::size_t firstSurface=24+64*32+96*4;
    bad=originalDual;bad[firstSurface]=1;mustReject(bad); // Missing first index
    bad=originalDual;bad[firstSurface+4]=3;mustReject(bad); // Triangle coverage
    bad=originalDual;bad[firstSurface+8]=0;mustReject(bad); // Invalid image dims
    bad=originalDual;bad.pop_back();mustReject(bad); // Truncation
    bad=originalDual;bad.push_back('!');mustReject(bad); // Trailing junk
    std::error_code ec;std::filesystem::remove(temp,ec);
    std::cout<<"RTSMESH1+2: legacy compatibility, 2 material surfaces, "
                "image/header/range corruption rejected\n";
}
