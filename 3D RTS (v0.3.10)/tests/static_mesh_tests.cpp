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
    if(argc!=3)throw std::invalid_argument("Expected project path and mesh path");
    const std::filesystem::path project=argv[1],source=argv[2];
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
    std::error_code ec;std::filesystem::remove(temp,ec);
    std::cout<<"RTSMESH1: validated original static mesh, image and secure bounds\n";
}
