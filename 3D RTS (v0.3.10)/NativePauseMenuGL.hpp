#pragma once
#include "NativePauseMenu.hpp"
#include "BitmapHUD.hpp"
#include <string>

// Native retained layout, custom engine renderer, no UI dependency.
namespace NativeUI {
class PauseMenuGL {
    GLuint shader_,vao_,vbo_;
    void box(const PauseMenu& m,Rect r,float red,float green,float blue,float alpha)const {
        if(r.w<=0.f || r.h<=0.f)return;
        const float x0=2.f*r.x/m.width()-1.f,x1=2.f*(r.x+r.w)/m.width()-1.f;
        const float y0=1.f-2.f*r.y/m.height(),y1=1.f-2.f*(r.y+r.h)/m.height();
        const float vertices[]={x0,y0,x1,y0,x1,y1,x0,y0,x1,y1,x0,y1};
        glUseProgram(shader_);
        glUniform4f(glGetUniformLocation(shader_,"uColor"),red,green,blue,alpha);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES,0,6);
    }
    void text(const PauseMenu& m,float x,float y,float size,const std::string& s,
              float r,float g,float b)const{
        drawHUDText(shader_,vao_,vbo_,m.width(),m.height(),x,y,size,s,r,g,b);
    }
public:
    PauseMenuGL(GLuint shader,GLuint vao,GLuint vbo):shader_(shader),vao_(vao),vbo_(vbo){}
    void draw(const PauseMenu& m,double mouseX,double mouseY,
              std::size_t focusIndex=0)const{
        const int hoveredIndex=m.indexAt(mouseX,mouseY);
        std::size_t buttonIndex=0;
        box(m,{0.f,0.f,float(m.width()),float(m.height())},.012f,.021f,.031f,.82f);
        const auto p=m.bounds;
        box(m,p,.036f,.059f,.083f,.99f);
        box(m,{p.x,p.y,p.w,4.f},.22f,.71f,.78f,1.f);
        text(m,p.x+24.f,p.y+23.f,3.f,"MATCH PAUSED",.92f,.96f,.97f);
        text(m,p.x+24.f,p.y+58.f,1.7f,"UP/DOWN SELECT | ENTER CONFIRM | ESC RESUME",.63f,.79f,.86f);
        for(const auto& b:m.buttons()){
            const bool hover=hoveredIndex>=0
                ?hoveredIndex==static_cast<int>(buttonIndex)
                :buttonIndex==focusIndex;
            ++buttonIndex;
            box(m,b.bounds,hover?.16f:.085f,hover?.31f:.16f,hover?.37f:.21f,.98f);
            box(m,{b.bounds.x,b.bounds.y,4.f,b.bounds.h},hover?.96f:.20f,
                hover?.73f:.51f,hover?.34f:.58f,1.f);
            text(m,b.bounds.x+15.f,b.bounds.y+std::max(3.f,(b.bounds.h-14.f)*.5f),
                 2.f,b.text,hover?1.f:.86f,hover?.93f:.89f,hover?.77f:.92f);
        }
    }
};
}
