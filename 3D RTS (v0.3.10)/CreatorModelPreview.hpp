#pragma once
// Isolated model viewport using the production textured mesh renderer.
#include "RTSStaticMeshGL.hpp"
#include "RTSAssetLibrary.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
namespace CreatorModelPreview {
inline constexpr GLenum FB=0x8D40,DFB=0x8CA9,RFB=0x8CA8,DB=0x8CA6,RDB=0x8CAA,
 RB=0x8D41,RBB=0x8CA7,COLOR=0x8CE0,DEPTH=0x8D00,D24=0x81A6,OK=0x8CD5;
struct State {
    GLint draw=0,read=0,rb=0,vp[4]{},program=0,vao=0,buffer=0,active=0,tex=0,unpack=4,func=GL_LESS;
    GLboolean mask=GL_TRUE,cm[4]{};
    GLfloat clear[4]{}; GLdouble depthClear=1;
    bool depth,blend,cull,scissor;
    State(){
        glGetIntegerv(DB,&draw);glGetIntegerv(RDB,&read);glGetIntegerv(RBB,&rb);
        glGetIntegerv(GL_VIEWPORT,vp);glGetIntegerv(0x8B8D,&program);
        glGetIntegerv(0x85B5,&vao);glGetIntegerv(0x8894,&buffer);
        glGetIntegerv(0x84E0,&active);rtsGlActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex);glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);
        glGetIntegerv(GL_DEPTH_FUNC,&func);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
        glGetBooleanv(GL_COLOR_WRITEMASK,cm);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE,&depthClear);
        depth=glIsEnabled(GL_DEPTH_TEST);blend=glIsEnabled(GL_BLEND);
        cull=glIsEnabled(GL_CULL_FACE);scissor=glIsEnabled(GL_SCISSOR_TEST);
    }
    static void enable(GLenum flag,bool b){if(b)glEnable(flag);else glDisable(flag);}
    ~State(){
        rtsGlBindFramebuffer(DFB,GLuint(draw));rtsGlBindFramebuffer(RFB,GLuint(read));
        rtsGlBindRenderbuffer(RB,GLuint(rb));glViewport(vp[0],vp[1],vp[2],vp[3]);
        glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));
        rtsGlActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,GLuint(tex));rtsGlActiveTexture(GLenum(active));
        glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);glDepthFunc(GLenum(func));glDepthMask(mask);
        glColorMask(cm[0],cm[1],cm[2],cm[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);glClearDepth(depthClear);
        enable(GL_DEPTH_TEST,depth);enable(GL_BLEND,blend);enable(GL_CULL_FACE,cull);enable(GL_SCISSOR_TEST,scissor);
    }
};
class Viewer {
    GLuint fb=0,color=0,depth=0;
    int width=0,height=0;
    std::string key;
    std::uint64_t revision=0;
    RTSStaticMeshRenderer renderer;
    float yaw=35,pitch=22,zoom=3.5f;
    bool bounds=true,axes=true,highlight=false;
    void reset(){yaw=35;pitch=22;zoom=3.5f;}
    bool allocate(int w,int h){
        if(fb && width==w && height==h)return true;
        if(!fb){rtsGlGenFramebuffers(1,&fb);glGenTextures(1,&color);rtsGlGenRenderbuffers(1,&depth);}
        rtsGlBindFramebuffer(FB,fb);glBindTexture(GL_TEXTURE_2D,color);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,0x8058,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        rtsGlFramebufferTexture2D(FB,COLOR,GL_TEXTURE_2D,color,0);
        rtsGlBindRenderbuffer(RB,depth);rtsGlRenderbufferStorage(RB,D24,w,h);
        rtsGlFramebufferRenderbuffer(FB,DEPTH,RB,depth);
        if(rtsGlCheckFramebufferStatus(FB)!=OK){width=height=0;return false;}
        width=w;height=h;return true;
    }
public:
    void clear(){
        renderer.clear();if(fb)rtsGlDeleteFramebuffers(1,&fb);
        if(depth)rtsGlDeleteRenderbuffers(1,&depth);if(color)glDeleteTextures(1,&color);
        fb=color=depth=0;width=height=0;key.clear();
    }
    void draw(const RTSAssetRecord& asset,const std::string& project,std::uint64_t generation,std::size_t selected){
        const std::string next=project+"|"+asset.reference;
        const float dw=std::max(120.f,ImGui::GetContentRegionAvail().x),
                    dh=std::clamp(dw*.67f,190.f,350.f);
        const int w=std::clamp(int(dw),120,1024),h=std::clamp(int(dh),120,768);
        if(next!=key)reset();
        Vec3 lo(asset.minimum[0],asset.minimum[1],asset.minimum[2]),hi(asset.maximum[0],asset.maximum[1],asset.maximum[2]);
        const Vec3 center=(lo+hi)*.5f;
        const float radius=std::max(.0001f,(hi-lo).length()*.5f);
        const Mat4 model=Mat4::scale(Vec3(1/radius,1/radius,1/radius))*Mat4::translate(center*-1.f);
        const float y=toRadians(yaw),p=toRadians(pitch);
        const Vec3 eye(zoom*std::cos(p)*std::sin(y),zoom*std::sin(p),zoom*std::cos(p)*std::cos(y));
        const Mat4 view=Mat4::lookAt(eye,Vec3(),Vec3(0,1,0)),proj=Mat4::perspective(toRadians(43),float(w)/h,.05f,40);
        bool rendered=false;
        {
            State guard;
            if(next!=key || revision!=generation){renderer.clear();key=next;revision=generation;}
            if(allocate(w,h)){
                rtsGlBindFramebuffer(FB,fb);glViewport(0,0,w,h);
                glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
                glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
                glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(.055f,.078f,.105f,1);
                glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                int first=-1,count=0;
                if(highlight && selected<asset.nodes.size()){
                    first=int(asset.nodes[selected].firstSurface);count=int(asset.nodes[selected].surfaceCount);
                }
                rendered=renderer.draw(asset.reference,project,model,view,proj,Vec3(1,1,1),1,first,count);
            }
        }
        if(!rendered){ImGui::TextWrapped("Model viewport could not render this asset.");return;}
        ImGui::Image((ImTextureID)(intptr_t)color,ImVec2(dw,dh),ImVec2(0,1),ImVec2(1,0));
        const ImVec2 start=ImGui::GetItemRectMin(),end=ImGui::GetItemRectMax();
        if(ImGui::IsItemHovered()){
            const auto& io=ImGui::GetIO();
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
                yaw=std::remainder(yaw+io.MouseDelta.x*.6f,360.f);pitch=std::clamp(pitch+io.MouseDelta.y*.5f,-85.f,85.f);
            }
            zoom=std::clamp(zoom*std::pow(.86f,io.MouseWheel),1.8f,12.f);
        }
        const Mat4 mvp=proj*view*model;
        const auto projectPoint=[&](Vec3 v,ImVec2& out){
            const Vec4 clip=mvp*Vec4(v.x,v.y,v.z,1);if(clip.w<=.001f)return false;
            out=ImVec2(start.x+(clip.x/clip.w*.5f+.5f)*dw,start.y+(.5f-clip.y/clip.w*.5f)*dh);
            return std::isfinite(out.x)&&std::isfinite(out.y);
        };
        auto* dl=ImGui::GetWindowDrawList();dl->PushClipRect(start,end,true);
        const auto line=[&](Vec3 a,Vec3 b,ImU32 c,float t){ImVec2 u,v;if(projectPoint(a,u)&&projectPoint(b,v))dl->AddLine(u,v,c,t);};
        if(bounds){
            Vec3 corners[8];for(int i=0;i<8;++i)corners[i]=Vec3((i&1)?hi.x:lo.x,(i&2)?hi.y:lo.y,(i&4)?hi.z:lo.z);
            for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit))line(corners[i],corners[i|bit],IM_COL32(105,170,205,155),1);
        }
        if(axes && selected<asset.nodes.size()){
            const auto& m=asset.nodes[selected].model;const Vec3 o(m[12],m[13],m[14]);
            const ImU32 colors[]={IM_COL32(255,90,90,255),IM_COL32(90,240,130,255),IM_COL32(75,155,255,255)};
            for(int a=0;a<3;++a)line(o,o+Vec3(m[a*4],m[a*4+1],m[a*4+2]).normalized()*(radius*.32f),colors[a],2);
            ImVec2 pt;if(projectPoint(o,pt))dl->AddCircleFilled(pt,3,IM_COL32(255,220,90,255));
        }
        dl->PopClipRect();ImGui::TextDisabled("Drag to orbit | wheel to zoom");
        if(ImGui::SmallButton("Frame model"))reset();ImGui::SameLine();ImGui::Checkbox("Bounds",&bounds);
        ImGui::SameLine();ImGui::Checkbox("Node axes",&axes);ImGui::Checkbox("Highlight node surfaces",&highlight);
    }
};
inline Viewer viewer;
}
