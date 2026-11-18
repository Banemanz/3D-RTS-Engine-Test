#include "RTSGltfImport.hpp"
#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 2048
#include "third_party/stb_image.h"
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <set>

namespace {
constexpr std::size_t maxBytes=64*1024*1024;
void need(bool condition,const char* message){
    if(!condition)throw std::invalid_argument(message);
}
std::vector<unsigned char> readFile(const std::filesystem::path& path){
    const auto size=std::filesystem::file_size(path);
    need(size>0 && size<=maxBytes,"Missing, empty or oversized source resource (64 MiB limit)");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    std::ifstream stream(path,std::ios::binary);
    need(bool(stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size())),"Cannot read source resource");
    return bytes;
}
// Strict base64 length/padding validation before any decoder reads the input.
std::vector<unsigned char> resource(const char* uri,const std::filesystem::path& source){
    need(uri && *uri,"Missing resource URI");
    const std::string name(uri);
    if(name.compare(0,5,"data:")==0){
        const auto start=name.find(";base64,");
        need(start!=std::string::npos,"Only base64 data URIs are supported");
        const std::string encoded=name.substr(start+8);
        need(!encoded.empty() && encoded.size()%4==0 && encoded.size()<=maxBytes*4/3+4,
             "Invalid or oversized base64 data");
        std::vector<unsigned char> out;
        out.reserve(encoded.size()/4*3);
        const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for(std::size_t i=0;i<encoded.size();i+=4){
            unsigned value=0,padding=0;
            for(int k=0;k<4;++k){
                const char c=encoded[i+k];
                if(c=='='){
                    need(i+4==encoded.size() && k>=2,"Invalid base64 padding");
                    ++padding;value<<=6;
                }else{
                    const auto digit=alphabet.find(c);
                    need(!padding && digit!=std::string::npos,"Invalid base64 character");
                    value=(value<<6)|static_cast<unsigned>(digit);
                }
            }
            out.push_back(static_cast<unsigned char>(value>>16));
            if(padding<2)out.push_back(static_cast<unsigned char>(value>>8));
            if(padding<1)out.push_back(static_cast<unsigned char>(value));
        }
        need(out.size()<=maxBytes,"Embedded resource exceeds 64 MiB");
        return out;
    }
    need(name.find_first_of(":%\\")==std::string::npos && name[0]!='/',
         "External resource URI must be a relative local path (no URL or escapes)");
    const auto root=std::filesystem::weakly_canonical(std::filesystem::absolute(source).parent_path());
    const auto path=std::filesystem::weakly_canonical(root/std::filesystem::u8path(name));
    const auto rel=path.lexically_relative(root);
    need(!rel.empty() && !rel.is_absolute(),"Resource escapes source directory");
    for(const auto& part:rel)need(part!="..","Resource escapes source directory");
    return readFile(path);
}
void checkAccessor(const cgltf_accessor& a){
    need(!a.is_sparse && a.buffer_view,"Sparse or absent accessors must be baked before import");
    const auto& v=*a.buffer_view;
    need(v.buffer && v.buffer->data && !v.has_meshopt_compression,"Invalid or compressed bufferView");
    need(v.offset<=v.buffer->size && v.size<=v.buffer->size-v.offset,"bufferView exceeds buffer");
    const auto item=cgltf_calc_size(a.type,a.component_type);
    need(item && a.count>0 && a.count<=750000 && a.stride>=item && a.stride<=252,
         "Invalid accessor type, count or stride");
    need(a.offset<=v.size && item<=v.size-a.offset &&
         a.count-1<=(v.size-a.offset-item)/a.stride,"Accessor exceeds bufferView");
    need(!a.normalized || (a.component_type==cgltf_component_type_r_8u ||
         a.component_type==cgltf_component_type_r_16u),"Invalid normalized accessor");
}
void writeMesh(const RTSStaticMesh& mesh,const std::filesystem::path& path){
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    need(bool(out),"Unable to create compiled asset");
    const auto write=[&](const void* data,std::size_t n){
        out.write(static_cast<const char*>(data),static_cast<std::streamsize>(n));
    };
    if(mesh.surfaces.empty()){
        // Preserve exact RTSMESH1 bytes for every previously imported asset.
        write("RTSMESH1",8);
        const std::uint32_t counts[]={
            static_cast<std::uint32_t>(mesh.vertices.size()/8),
            static_cast<std::uint32_t>(mesh.indices.size()),mesh.width,mesh.height};
        write(counts,sizeof(counts));write(mesh.baseColor.data(),16);
        write(mesh.vertices.data(),mesh.vertices.size()*sizeof(float));
        write(mesh.indices.data(),mesh.indices.size()*sizeof(std::uint32_t));
        if(!mesh.albedo.empty())write(mesh.albedo.data(),mesh.albedo.size());
    }else{
        // RTSMESH2: magic, geometry/primitive counts, indexed geometry,
        // then contiguous draw ranges with independent opaque RGBA materials.
        write("RTSMESH2",8);
        const std::uint32_t counts[]={
            static_cast<std::uint32_t>(mesh.vertices.size()/8),
            static_cast<std::uint32_t>(mesh.indices.size()),
            static_cast<std::uint32_t>(mesh.surfaces.size()),0};
        write(counts,sizeof(counts));
        write(mesh.vertices.data(),mesh.vertices.size()*sizeof(float));
        write(mesh.indices.data(),mesh.indices.size()*sizeof(std::uint32_t));
        for(const auto& surface:mesh.surfaces){
            const std::uint32_t fields[]={
                surface.firstIndex,surface.indexCount,surface.width,surface.height};
            write(fields,sizeof(fields));
            write(surface.baseColor.data(),16);
            if(!surface.albedo.empty())
                write(surface.albedo.data(),surface.albedo.size());
        }
    }
    out.close();need(bool(out),"Unable to finish compiled asset");
}
}

RTSStaticMesh rtsCompileGltf(const std::filesystem::path& source){
    auto ext=source.extension().string();
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    need(ext==".gltf" || ext==".glb","Choose a .gltf or .glb model");
    const auto raw=readFile(source);
    // Bound parser allocations as well as input bytes. Monotonic accounting
    // also limits allocation churn on malformed documents.
    std::size_t allocated=0;
    cgltf_options options{};
    options.memory.user_data=&allocated;
    options.memory.alloc_func=[](void* user,cgltf_size size)->void*{
        auto& used=*static_cast<std::size_t*>(user);
        if(size>2*maxBytes-used)return nullptr;
        used+=size;return std::malloc(size);
    };
    options.memory.free_func=[](void*,void* ptr){std::free(ptr);};
    cgltf_data* parsed=nullptr;
    need(cgltf_parse(&options,raw.data(),raw.size(),&parsed)==cgltf_result_success,"Invalid glTF 2.0 / GLB document");
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(parsed,cgltf_free);
    auto& d=*parsed;
    need(d.extensions_used_count==0 && d.extensions_required_count==0,"glTF extensions are not supported by this static compiler");
    need(!d.skins_count && !d.animations_count,"Skins and animation require the future animated asset pipeline");
    need(d.meshes_count==1 && d.meshes[0].primitives_count>=1 &&
         d.meshes[0].primitives_count<=32,
         "Static import supports one mesh containing 1..32 triangle primitives");
    need(d.nodes_count==1 && d.nodes[0].mesh==&d.meshes[0] &&
         !d.nodes[0].children_count && !d.nodes[0].skin && !d.nodes[0].has_mesh_gpu_instancing,
         "Export one root mesh node; scene hierarchies are not yet supported");
    need(d.scenes_count<=1 && (!d.scenes_count ||
         (d.scenes[0].nodes_count==1 && d.scenes[0].nodes[0]==&d.nodes[0])),"Export a single scene containing the mesh");
    need(!d.cameras_count && !d.lights_count,"Export mesh selection without cameras or lights");
    need(d.buffers_count>0 && d.buffers_count<=32 && d.buffer_views_count<=256 && d.accessors_count<=256,
         "Too many buffers, views or accessors");
    // cgltf borrows these bytes without freeing them; never let it open URI paths.
    std::vector<std::vector<unsigned char>> buffers(d.buffers_count);
    std::size_t total=0;
    for(std::size_t i=0;i<d.buffers_count;++i){
        auto& b=d.buffers[i];
        need(b.size>0 && b.size<=maxBytes-total,"Combined buffers exceed 64 MiB");
        total+=b.size;
        if(b.uri)buffers[i]=resource(b.uri,source);
        else{
            need(i==0 && d.bin && d.bin_size>=b.size,"Missing GLB binary data");
            const auto* p=static_cast<const unsigned char*>(d.bin);
            buffers[i].assign(p,p+b.size);
        }
        need(buffers[i].size()>=b.size && buffers[i].size()-b.size<=3,"Truncated buffer or excess resource bytes");
        b.data=buffers[i].data();b.data_free_method=cgltf_data_free_method_none;
    }
    for(std::size_t i=0;i<d.buffer_views_count;++i){
        const auto& v=d.buffer_views[i];
        need(v.buffer && v.offset<=v.buffer->size && v.size<=v.buffer->size-v.offset &&
             !v.has_meshopt_compression,"Invalid or compressed bufferView");
    }
    // Preflight arithmetic before calling parser helpers that read accessor memory.
    for(std::size_t i=0;i<d.accessors_count;++i)checkAccessor(d.accessors[i]);
    need(cgltf_validate(&d)==cgltf_result_success,"glTF validation failed");
    const auto& node=d.nodes[0];
    need(!node.has_matrix,"Bake node matrices to TRS before import");
    double rotationLength=0;
    for(float f:node.rotation){
        need(std::isfinite(f),"Invalid node rotation");
        rotationLength+=double(f)*f;
    }
    need(std::abs(rotationLength-1)<0.001,"Node quaternion must be normalized");
    for(float f:node.scale)
        need(std::isfinite(f) && f>0,"Node scale must be finite and positive");
    float matrix[16];
    cgltf_node_transform_local(&node,matrix);
    for(float f:matrix)need(std::isfinite(f),"Invalid node transform");

    RTSStaticMesh result;
    result.surfaces.reserve(d.meshes[0].primitives_count);
    for(std::size_t primitiveIndex=0;
        primitiveIndex<d.meshes[0].primitives_count;++primitiveIndex){
        const auto& p=d.meshes[0].primitives[primitiveIndex];
        need(p.type==cgltf_primitive_type_triangles &&
             !p.has_draco_mesh_compression && !p.targets_count &&
             !d.meshes[0].weights_count,
             "Only static triangle primitives are supported");
        const cgltf_accessor *positions=nullptr,*normals=nullptr,*uvs=nullptr;
        for(std::size_t i=0;i<p.attributes_count;++i){
            const auto& a=p.attributes[i];
            if(a.type==cgltf_attribute_type_position && a.index==0 && !positions)
                positions=a.data;
            else if(a.type==cgltf_attribute_type_normal && a.index==0 && !normals)
                normals=a.data;
            else if(a.type==cgltf_attribute_type_texcoord && a.index==0 && !uvs)
                uvs=a.data;
            else need(false,"Export POSITION, NORMAL, optional TEXCOORD_0 only; remove tangents/colors/extra UVs");
        }
        need(positions && normals,"Export vertex positions and normals");
        need(positions->type==cgltf_type_vec3 &&
             positions->component_type==cgltf_component_type_r_32f &&
             normals->type==cgltf_type_vec3 &&
             normals->component_type==cgltf_component_type_r_32f &&
             positions->count==normals->count &&
             positions->count>=3 && positions->count<=250000,
             "Invalid POSITION/NORMAL layout or vertex count");
        need(!uvs || (uvs->type==cgltf_type_vec2 && uvs->count==positions->count &&
             (uvs->component_type==cgltf_component_type_r_32f || uvs->normalized)),
             "Invalid UV0 layout");
        const auto indexCount=p.indices?p.indices->count:positions->count;
        need(indexCount>=3 && indexCount<=750000 && indexCount%3==0,
             "Invalid triangle index count");
        if(p.indices)
            need(p.indices->type==cgltf_type_scalar && !p.indices->normalized &&
                 (p.indices->component_type==cgltf_component_type_r_8u ||
                  p.indices->component_type==cgltf_component_type_r_16u ||
                  p.indices->component_type==cgltf_component_type_r_32u),
                 "Triangle indices must be unsigned integers");
        const auto baseVertex=result.vertices.size()/8;
        const auto firstIndex=result.indices.size();
        need(baseVertex+positions->count<=250000 &&
             firstIndex+indexCount<=750000,
             "Combined primitive geometry exceeds static mesh limits");
        for(std::size_t i=0;i<indexCount;++i){
            const auto index=p.indices?cgltf_accessor_read_index(p.indices,i):i;
            need(index<positions->count,"Triangle index exceeds primitive vertices");
            result.indices.push_back(static_cast<std::uint32_t>(baseVertex+index));
        }
        for(std::size_t i=0;i<positions->count;++i){
            float v[3],n[3],uv[2]={0,0};
            need(cgltf_accessor_read_float(positions,i,v,3) &&
                 cgltf_accessor_read_float(normals,i,n,3),"Cannot read vertex");
            if(uvs)need(cgltf_accessor_read_float(uvs,i,uv,2),"Cannot read UV");
            float out[8]={0,0,0,0,0,0,uv[0],uv[1]};
            for(int r=0;r<3;++r){
                out[r]=matrix[12+r];
                for(int c=0;c<3;++c){
                    out[r]+=matrix[c*4+r]*v[c];
                    out[3+r]+=matrix[c*4+r]*n[c]/(node.scale[c]*node.scale[c]);
                }
            }
            const double length=std::sqrt(double(out[3])*out[3]+
                                          double(out[4])*out[4]+
                                          double(out[5])*out[5]);
            need(length>1e-12 && std::isfinite(length),
                 "Invalid zero-length normal");
            for(int j=3;j<6;++j)
                out[j]=static_cast<float>(out[j]/length);
            for(float f:out)
                need(std::isfinite(f) && std::abs(f)<1e7f,
                     "Invalid or oversized transformed vertex");
            result.vertices.insert(result.vertices.end(),out,out+8);
        }
        RTSStaticSurface surface;
        surface.firstIndex=static_cast<std::uint32_t>(firstIndex);
        surface.indexCount=static_cast<std::uint32_t>(indexCount);
        if(p.material){
            const auto& m=*p.material;
            need(!m.has_pbr_specular_glossiness && !m.has_clearcoat &&
                 !m.has_transmission && !m.has_volume && !m.has_ior &&
                 !m.has_specular && !m.has_sheen && !m.has_emissive_strength &&
                 !m.has_iridescence && !m.has_diffuse_transmission &&
                 !m.has_anisotropy && !m.has_dispersion && !m.unlit &&
                 !m.extensions_count,"Extended materials are not supported");
            need(m.alpha_mode==cgltf_alpha_mode_opaque && !m.double_sided,
                 "Only opaque, single-sided materials are supported");
            need(!m.normal_texture.texture && !m.occlusion_texture.texture &&
                 !m.emissive_texture.texture &&
                 m.emissive_factor[0]==0 && m.emissive_factor[1]==0 &&
                 m.emissive_factor[2]==0 &&
                 !m.pbr_metallic_roughness.metallic_roughness_texture.texture,
                 "Normal, occlusion, emissive and metallic/roughness maps need the future material pipeline");
            const auto& material=m.pbr_metallic_roughness;
            std::copy(material.base_color_factor,
                      material.base_color_factor+4,surface.baseColor.begin());
            for(float f:surface.baseColor)
                need(std::isfinite(f) && f>=0 && f<=1,
                     "Invalid material base color");
            surface.baseColor[3]=1.f; // glTF OPAQUE ignores alpha
            const auto& view=material.base_color_texture;
            if(view.texture){
                need(uvs && view.texcoord==0 && !view.has_transform,
                     "Texture requires UV0 without texture transforms");
                const auto& tex=*view.texture;
                need(tex.image && !tex.has_basisu && !tex.has_webp,
                     "Missing or unsupported base color image");
                if(tex.sampler)
                    need(tex.sampler->wrap_s==cgltf_wrap_mode_repeat &&
                         tex.sampler->wrap_t==cgltf_wrap_mode_repeat &&
                         (!tex.sampler->mag_filter ||
                          tex.sampler->mag_filter==cgltf_filter_type_linear) &&
                         (!tex.sampler->min_filter ||
                          tex.sampler->min_filter==cgltf_filter_type_linear),
                         "Only repeat wrapping with linear filtering is supported");
                std::vector<unsigned char> bytes;
                if(tex.image->uri)bytes=resource(tex.image->uri,source);
                else{
                    need(tex.image->buffer_view,"Image has no URI or bufferView");
                    const auto& v=*tex.image->buffer_view;
                    need(v.buffer && v.buffer->data && v.size<=maxBytes,
                         "Invalid image bufferView");
                    const auto* b=
                        static_cast<const unsigned char*>(v.buffer->data)+v.offset;
                    bytes.assign(b,b+v.size);
                }
                int w=0,h=0,channels=0;
                need(stbi_info_from_memory(bytes.data(),
                         static_cast<int>(bytes.size()),&w,&h,&channels) &&
                     w>0 && h>0 && w<=2048 && h<=2048,
                     "Invalid PNG/JPEG image or dimensions exceed 2048");
                stbi_uc* pixels=stbi_load_from_memory(
                    bytes.data(),static_cast<int>(bytes.size()),&w,&h,&channels,4);
                need(pixels,"PNG/JPEG decode failed");
                std::unique_ptr<stbi_uc,decltype(&stbi_image_free)>
                    image(pixels,stbi_image_free);
                surface.width=static_cast<std::uint32_t>(w);
                surface.height=static_cast<std::uint32_t>(h);
                surface.albedo.assign(
                    pixels,pixels+static_cast<std::size_t>(w)*h*4);
                for(std::size_t i=3;i<surface.albedo.size();i+=4)
                    surface.albedo[i]=255; // opaque alpha contract
            }
        }
        result.surfaces.emplace_back(std::move(surface));
    }
    if(result.surfaces.size()==1){
        // Single primitive is deliberately serialized in historic RTSMESH1.
        // Native import of all old samples must remain bit-identical.
        auto& surface=result.surfaces.front();
        result.width=surface.width;
        result.height=surface.height;
        result.baseColor=surface.baseColor;
        result.albedo=std::move(surface.albedo);
        result.surfaces.clear();
    }
    return result;
}

void rtsImportGltf(const std::filesystem::path& source,const std::filesystem::path& project,
                   const std::string& reference){
    const auto destination=rtsResolveMeshReference(project,reference);
    need(!std::filesystem::exists(destination),"Asset already exists; choose a different output name");
    const auto mesh=rtsCompileGltf(source); // no output mutations on invalid input
    std::filesystem::create_directories(destination.parent_path());
    // Reserve an isolated staging directory; hard-link publication is atomic and
    // fails if another importer has already created the destination (no clobber).
    auto staging=destination;staging+=".importing";
    need(std::filesystem::create_directory(staging),"Import already in progress for this asset");
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{staging};
    const auto temp=staging/"mesh.tmp";
    writeMesh(mesh,temp);
    rtsReadStaticMesh(temp); // compiler output must satisfy the runtime contract
    std::filesystem::create_hard_link(temp,destination);
}
