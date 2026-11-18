#pragma once
// OpenGL presentation-only, first real asset path. No gameplay mutations.
// Static glTF is precompiled to bounded RTSMESH1 for cheap, cached uploads.
#include "GLCore.hpp"
#include "Math3D.hpp"
#include "RTSStaticMesh.hpp"
#include "Shaders.hpp"
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <utility>

inline const char* RTS_IMPORTED_VERTEX = R"(
#version 330 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aNormal;
layout (location=2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vNormal;
out vec2 vUV;
void main(){
    // Correct nonuniform world/placement scaling (inverse transpose).
    vNormal=transpose(inverse(mat3(uModel)))*aNormal;
    vUV=aUV;
    gl_Position=uProjection*uView*uModel*vec4(aPos,1.0);
}
)";
inline const char* RTS_IMPORTED_FRAGMENT = R"(
#version 330 core
in vec3 vNormal;
in vec2 vUV;
uniform vec4 uBaseColor;
uniform vec3 uActorTint;
uniform float uOpacity;
uniform bool uHasAlbedo;
uniform sampler2D uAlbedo;
out vec4 FragColor;
void main(){
    vec4 albedo=uBaseColor;
    if(uHasAlbedo)albedo*=texture(uAlbedo,vUV);
    vec3 lightDir=normalize(vec3(0.45,0.85,0.35));
    float diffuse=max(dot(normalize(vNormal),lightDir),0.0);
    FragColor=vec4(albedo.rgb*uActorTint*(0.38+0.72*diffuse),
                   albedo.a*uOpacity);
}
)";
class RTSStaticMeshRenderer {
    struct GPUSurface {
        GLuint texture=0;
        GLsizei firstIndex=0,indexCount=0;
        std::array<float,4> color{{1,1,1,1}};
    };
    struct Asset {
        GLuint vao=0,vbo=0,ebo=0;
        GLsizei triangles=0;
        std::vector<GPUSurface> surfaces;
        bool attempted=false,ready=false;
    };
    std::map<std::string,Asset> assets_;
    GLuint shader_=0;

    bool load(Asset& a,const std::filesystem::path& filename){
        a.attempted=true;
        try {
            const RTSStaticMesh mesh=rtsReadStaticMesh(filename);
            glGenVertexArrays(1,&a.vao);
            glGenBuffers(1,&a.vbo);
            glGenBuffers(1,&a.ebo);
            glBindVertexArray(a.vao);
            glBindBuffer(GL_ARRAY_BUFFER,a.vbo);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(mesh.vertices.size()*sizeof(float)),
                         mesh.vertices.data(),GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,a.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(mesh.indices.size()*sizeof(std::uint32_t)),
                         mesh.indices.data(),GL_STATIC_DRAW);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),
                                  (void*)(3*sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),
                                  (void*)(6*sizeof(float)));
            glEnableVertexAttribArray(2);
            const auto upload=[&](std::uint32_t first,std::uint32_t count,
                                  std::uint32_t width,std::uint32_t height,
                                  const std::array<float,4>& color,
                                  const std::vector<unsigned char>& pixels){
                GPUSurface surface;
                surface.firstIndex=static_cast<GLsizei>(first);
                surface.indexCount=static_cast<GLsizei>(count);
                surface.color=color;
                if(!pixels.empty()){
                    glGenTextures(1,&surface.texture);
                    // Own the handle even if a later GL operation fails.
                    a.surfaces.emplace_back(surface);
                    auto& current=a.surfaces.back();
                    rtsGlActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D,current.texture);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
                    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,
                        static_cast<GLsizei>(width),
                        static_cast<GLsizei>(height),0,
                        GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                }else{
                    a.surfaces.emplace_back(surface);
                }
            };
            if(mesh.surfaces.empty()){
                upload(0,static_cast<std::uint32_t>(mesh.indices.size()),
                       mesh.width,mesh.height,mesh.baseColor,mesh.albedo);
            }else{
                for(const auto& part:mesh.surfaces)
                    upload(part.firstIndex,part.indexCount,part.width,
                           part.height,part.baseColor,part.albedo);
            }
            a.triangles=static_cast<GLsizei>(mesh.indices.size());
            a.ready=true;
            if(!shader_)shader_=compileShaderProgram(RTS_IMPORTED_VERTEX,
                                                      RTS_IMPORTED_FRAGMENT);
            std::cerr<<"[Assets] Imported static mesh "<<filename.string()
                     <<" ("<<a.triangles/3<<" triangles)\n";
        }catch(const std::exception& e){
            std::cerr<<"[Assets] "<<filename.string()<<": "<<e.what()
                     <<" (using procedural fallback)\n";
            a.ready=false;
        }
        return a.ready;
    }
public:
    RTSStaticMeshRenderer()=default;
    RTSStaticMeshRenderer(const RTSStaticMeshRenderer&)=delete;
    RTSStaticMeshRenderer& operator=(const RTSStaticMeshRenderer&)=delete;
    bool draw(const std::string& reference,
              const std::filesystem::path& projectFile,
              const Mat4& model,const Mat4& view,const Mat4& projection,
              const Vec3& tint,float opacity){
        if(reference=="procedural.unit" || !rtsSafeMeshReference(reference))
            return false;
        // Cache by reference + project root, so reloading another map with
        // the same relative model name never accidentally reuses a different
        // project's mesh. File I/O is once per unique asset, not per frame.
        const std::string key=projectFile.lexically_normal().generic_string()+
                              "|" +reference;
        auto it=assets_.find(key);
        if(it==assets_.end()){
            Asset fresh;
            try{(void)load(fresh,rtsResolveMeshReference(projectFile,reference));}
            catch(const std::exception& e){
                fresh.attempted=true;
                std::cerr<<"[Assets] Invalid asset reference "<<reference
                         <<": "<<e.what()<<"\n";
            }
            it=assets_.emplace(key,std::move(fresh)).first;
        }
        const Asset& a=it->second;
        if(!a.ready)return false;
        glUseProgram(shader_);
        glUniformMatrix4fv(glGetUniformLocation(shader_,"uModel"),1,GL_FALSE,model.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_,"uView"),1,GL_FALSE,view.m);
        glUniformMatrix4fv(glGetUniformLocation(shader_,"uProjection"),1,
                            GL_FALSE,projection.m);
        glUniform3f(glGetUniformLocation(shader_,"uActorTint"),tint.x,tint.y,tint.z);
        glUniform1f(glGetUniformLocation(shader_,"uOpacity"),opacity);
        glUniform1i(glGetUniformLocation(shader_,"uAlbedo"),0);
        glBindVertexArray(a.vao);
        for(const auto& part:a.surfaces){
            glUniform4f(glGetUniformLocation(shader_,"uBaseColor"),
                        part.color[0],part.color[1],part.color[2],part.color[3]);
            glUniform1i(glGetUniformLocation(shader_,"uHasAlbedo"),part.texture?1:0);
            rtsGlActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,part.texture);
            glDrawElements(GL_TRIANGLES,part.indexCount,GL_UNSIGNED_INT,
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(part.firstIndex)*sizeof(std::uint32_t)));
        }
        return true;
    }
    // Must run before destroying GLFW's active context.
    void clear(){
        for(auto& kv:assets_){
            auto& a=kv.second;
            for(auto& surface:a.surfaces)
                if(surface.texture)glDeleteTextures(1,&surface.texture);
            if(a.vbo)glDeleteBuffers(1,&a.vbo);
            if(a.ebo)glDeleteBuffers(1,&a.ebo);
            if(a.vao)glDeleteVertexArrays(1,&a.vao);
        }
        assets_.clear();
        if(shader_){glDeleteProgram(shader_);shader_=0;}
    }
};
